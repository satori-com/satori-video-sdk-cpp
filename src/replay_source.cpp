#include "video_streams.h"

#include <fstream>
#include <gsl/gsl>
#include <json.hpp>
#include <string>
#include <thread>

#include "logging.h"
#include "streams/asio_streams.h"

namespace satori {
namespace video {

class read_json_impl {
 public:
  explicit read_json_impl(const std::string &filename)
      : _filename(filename), _input(filename) {}

  void generate_one(streams::observer<nlohmann::json> &observer) {
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
    } catch (const nlohmann::json::parse_error &e) {
      ABORT() << "Unable to parse line: " << e.what() << " " << line;
    }
    CHECK(data.is_object()) << "bad data: " << data;
    observer.on_next(std::move(data));
  }

 private:
  const std::string _filename;
  std::ifstream _input;
};

streams::publisher<nlohmann::json> read_json(const std::string &filename) {
  return streams::generators<nlohmann::json>::stateful(
      [filename]() { return new read_json_impl(filename); },
      [](read_json_impl *impl, streams::observer<nlohmann::json> &sink) {
        return impl->generate_one(sink);
      });
}

static streams::publisher<nlohmann::json> get_messages(nlohmann::json &&doc) {
  CHECK(doc.find("messages") != doc.end()) << "bad doc: " << doc;
  auto &messages = doc["messages"];
  CHECK(messages.is_array()) << "bad doc: " << doc;

  std::vector<nlohmann::json> result;
  for (auto &el : messages) {
    result.push_back(el);
  }

  return streams::publishers::of(std::move(result));
}

streams::publisher<network_packet> read_metadata(const std::string &metadata_file) {
  return read_json(metadata_file) >> streams::head()
         >> streams::map([](nlohmann::json &&t) {
             return network_packet{parse_network_metadata(t)};
           });
}

static double get_timestamp(const nlohmann::json &item) {
  CHECK(item.find("timestamp") != item.end()) << "bad item: " << item;
  auto &t = item["timestamp"];
  CHECK(t.is_number_float()) << "bad item: " << item;
  return t;
}

streams::publisher<network_packet> network_replay_source(boost::asio::io_service &io,
                                                         const std::string &filename,
                                                         bool batch) {
  auto metadata = read_metadata(filename + ".metadata");
  streams::publisher<nlohmann::json> items = read_json(filename);
  if (!batch) {
    auto last_time = new double{-1.0};
    items = std::move(items)
            >> streams::asio::delay(
                   io,
                   [last_time](const nlohmann::json &item) {
                     if (*last_time < 0) {
                       return std::chrono::milliseconds(0);
                     }

                     auto delay_ms = (int)((get_timestamp(item) - *last_time) * 1000);
                     return std::chrono::milliseconds(delay_ms);
                   })
            >> streams::map([last_time](nlohmann::json &&item) {
                *last_time = get_timestamp(item);
                return std::move(item);
              })
            >> streams::do_finally([last_time]() { delete last_time; });
  }
  auto frames = std::move(items) >> streams::flat_map(&get_messages)
                >> streams::map([](nlohmann::json &&t) {
                    return network_packet{parse_network_frame(t)};
                  });

  return streams::publishers::concat(std::move(metadata), std::move(frames));
}

}  // namespace video
}  // namespace satori
