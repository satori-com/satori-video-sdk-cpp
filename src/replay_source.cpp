#include <rapidjson/document.h>
#include <cstring>
#include <fstream>
#include <functional>
#include <gsl/gsl>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "asio_streams.h"
#include "logging.h"
#include "video_streams.h"

extern "C" {
#include <libavutil/error.h>
}

namespace rtm {
namespace video {

struct read_json_impl {
  explicit read_json_impl(const std::string &filename)
      : _filename(filename), _input(filename) {}

  void generate(int count, streams::observer<rapidjson::Document> &observer) {
    if (!_input.good()) {
      LOG(ERROR) << "replay file not found: " << _filename;
      observer.on_error(std::make_error_condition(std::errc::no_such_file_or_directory));
      return;
    }

    for (int sent = 0; sent < count; ++sent) {
      std::string line;
      if (!std::getline(_input, line)) {
        LOG(4) << "end of file";
        observer.on_complete();
        return;
      }
      LOG(4) << "line=" << line;

      rapidjson::Document data;
      data.Parse<0>(line.c_str()).HasParseError();
      CHECK(data.IsObject());
      observer.on_next(std::move(data));
    }
  }

  const std::string _filename;
  std::ifstream _input;
};

streams::publisher<rapidjson::Document> read_json(const std::string &filename) {
  return streams::generators<rapidjson::Document>::stateful(
      [filename]() { return new read_json_impl(filename); },
      [](read_json_impl *impl, int count, streams::observer<rapidjson::Document> &sink) {
        return impl->generate(count, sink);
      });
}

streams::publisher<network_packet> read_metadata(const std::string &metadata_file) {
  streams::publisher<rapidjson::Document> doc =
      read_json(metadata_file) >> streams::head();
  return std::move(doc) >> streams::map([](rapidjson::Document &&document) {
           const std::string name = document["codecName"].GetString();
           const std::string base64_data =
               document.HasMember("codecData") ? document["codecData"].GetString() : "";

           network_metadata m;
           m.codec_name = name;
           m.base64_data = base64_data;
           return network_packet{m};
         });
}

streams::publisher<network_packet> network_replay_source(boost::asio::io_service &io,
                                                         const std::string &filename,
                                                         bool batch) {
  auto metadata = read_metadata(filename + ".metadata");
  streams::publisher<rapidjson::Document> docs = read_json(filename);
  if (!batch) {
    double *last_time = new double{-1.0};
    docs = std::move(docs) >> streams::asio::delay(
                                  io,
                                  [last_time](const rapidjson::Document &doc) {
                                    if (*last_time < 0) {
                                      return std::chrono::milliseconds(0);
                                    }

                                    double timestamp = doc["timestamp"].GetDouble();
                                    int delay_ms = (int)((timestamp - *last_time) * 1000);
                                    return std::chrono::milliseconds(delay_ms);
                                  })
           >> streams::map([last_time](rapidjson::Document &&doc) {
               *last_time = doc["timestamp"].GetDouble();
               return std::move(doc);
             })
           >> streams::do_finally([last_time]() { delete last_time; });
  }
  auto frames = std::move(docs) >> streams::flat_map([](rapidjson::Document &&doc) {
                  std::vector<network_packet> packets;
                  for (const auto &msg : doc["messages"].GetArray()) {
                    const std::string base64_data = msg["d"].GetString();
                    const auto t = msg["i"].GetArray();
                    const int64_t i1 = t[0].GetInt64();
                    const int64_t i2 = t[1].GetInt64();

                    const double ntp_timestamp =
                        msg.HasMember("t") ? msg["t"].GetDouble() : 0;

                    uint32_t chunk = 1, chunks = 1;
                    if (msg.HasMember("c")) {
                      chunk = msg["c"].GetUint();
                      chunks = msg["l"].GetUint();
                    }

                    network_frame f;
                    f.base64_data = base64_data;
                    f.id = frame_id{i1, i2};
                    f.t = std::chrono::system_clock::from_time_t(ntp_timestamp);
                    f.chunk = chunk;
                    f.chunks = chunks;

                    packets.push_back(std::move(f));
                  }
                  return streams::publishers::of(std::move(packets));
                });

  return streams::publishers::merge(std::move(metadata), std::move(frames));
}

}  // namespace video
}  // namespace rtm
