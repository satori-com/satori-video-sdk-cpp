#include "video_streams.h"

#include <fstream>
#include <gsl/gsl>
#include <json.hpp>
#include <string>
#include <thread>

#include "cbor_json.h"
#include "cbor_tools.h"
#include "logging.h"
#include "streams/asio_streams.h"

namespace satori {
namespace video {

struct read_json_impl {
  explicit read_json_impl(const std::string &filename)
      : _filename(filename), _input(filename) {}

  void generate_one(streams::observer<cbor_item_t *> &observer) {
    if (!_input.good()) {
      LOG(ERROR) << "replay file not found: " << _filename;
      observer.on_error(std::make_error_condition(std::errc::no_such_file_or_directory));
      return;
    }

    std::string line;
    if (!std::getline(_input, line)) {
      LOG(4) << "end of file";
      observer.on_complete();
      return;
    }
    LOG(4) << "line=" << line;
    nlohmann::json data;
    try {
      data = nlohmann::json::parse(line);
    } catch (const std::exception &e) {
      ABORT() << "Unable to parse line: " << e.what() << " " << line;
    }
    CHECK(data.is_object());
    observer.on_next(cbor_move(json_to_cbor(data)));
  }

  const std::string _filename;
  std::ifstream _input;
};

streams::publisher<cbor_item_t *> read_json(const std::string &filename) {
  return streams::generators<cbor_item_t *>::stateful(
      [filename]() { return new read_json_impl(filename); },
      [](read_json_impl *impl, streams::observer<cbor_item_t *> &sink) {
        return impl->generate_one(sink);
      });
}

static streams::publisher<cbor_item_t *> get_messages(cbor_item_t *&&doc) {
  CHECK_EQ(0, cbor_refcount(doc));
  cbor_incref(doc);
  auto decref = gsl::finally([&doc]() { cbor_decref(&doc); });

  cbor_item_t *messages = cbor::map(doc).get("messages");
  CHECK(cbor_isa_array(messages));
  std::vector<cbor_item_t *> result;
  auto array_handle = cbor_array_handle(messages);
  for (size_t i = 0; i < cbor_array_size(messages); ++i) {
    result.push_back(cbor_incref(array_handle[i]));
  }
  return streams::publishers::of(std::move(result));
}

streams::publisher<network_packet> read_metadata(const std::string &metadata_file) {
  return read_json(metadata_file) >> streams::head()
         >> streams::map(&parse_network_metadata);
}

static double get_timestamp(const cbor_item_t *item) {
  return cbor_float_get_float8(cbor::map(item).get("timestamp"));
}

streams::publisher<network_packet> network_replay_source(boost::asio::io_service &io,
                                                         const std::string &filename,
                                                         bool batch) {
  auto metadata = read_metadata(filename + ".metadata");
  streams::publisher<cbor_item_t *> items = read_json(filename);
  if (!batch) {
    auto last_time = new double{-1.0};
    items = std::move(items)
            >> streams::asio::delay(
                   io,
                   [last_time](const cbor_item_t *item) {
                     if (*last_time < 0) {
                       return std::chrono::milliseconds(0);
                     }

                     auto delay_ms = (int)((get_timestamp(item) - *last_time) * 1000);
                     return std::chrono::milliseconds(delay_ms);
                   })
            >> streams::map([last_time](cbor_item_t *&&item) {
                *last_time = get_timestamp(item);
                return std::move(item);
              })
            >> streams::do_finally([last_time]() { delete last_time; });
  }
  auto frames = std::move(items) >> streams::flat_map(&get_messages)
                >> streams::map(&parse_network_frame);

  return streams::publishers::concat(std::move(metadata), std::move(frames));
}

}  // namespace video
}  // namespace satori
