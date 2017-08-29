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
#include "video_streams.h"

extern "C" {
#include <libavutil/error.h>
}

namespace rtm {
namespace video {

struct read_json_impl {
  explicit read_json_impl(const std::string &filename) : _input(filename) {}

  void generate(int count, streams::observer<rapidjson::Document> &observer) {
    for (int sent = 0; sent < count; ++sent) {
      std::string line;
      if (!std::getline(_input, line)) {
        observer.on_complete();
        return;
      }

      rapidjson::Document data;
      data.Parse<0>(line.c_str()).HasParseError();
      assert(data.IsObject());
      observer.on_next(std::move(data));
    }
  }

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

           return network_packet{
               network_metadata{.codec_name = name, .base64_data = base64_data}};
         });
}

streams::publisher<network_packet> network_replay_source(boost::asio::io_service &io,
                                                         const std::string &filename,
                                                         bool synchronous) {
  auto metadata = read_metadata(filename + ".metadata");
  streams::publisher<rapidjson::Document> docs = read_json(filename);
  if (synchronous) {
    double *last_time = new double{-1.0};
    docs = std::move(docs) >>
           streams::asio::delay(
               io,
               [last_time](const rapidjson::Document &doc) {
                 return std::chrono::milliseconds(
                     (int)((doc["timestamp"].GetDouble() - *last_time) * 1000));
               }) >>
           streams::map([last_time](rapidjson::Document &&doc) {
             *last_time = doc["timestamp"].GetDouble();
             return std::move(doc);
           }) >>
           streams::do_finally([last_time]() { delete last_time; });
  }
  auto frames =
      std::move(docs) >> streams::flat_map([](rapidjson::Document &&doc) {
        std::vector<network_packet> packets;
        for (const auto &msg : doc["messages"].GetArray()) {
          const std::string base64_data = msg["d"].GetString();
          const auto t = msg["i"].GetArray();
          const int64_t i1 = t[0].GetInt64();
          const int64_t i2 = t[1].GetInt64();

          const double ntp_timestamp = msg.HasMember("t") ? msg["t"].GetDouble() : 0;

          uint32_t chunk = 1, chunks = 1;
          if (msg.HasMember("c")) {
            chunk = msg["c"].GetUint();
            chunks = msg["l"].GetUint();
          }

          packets.push_back(
              network_frame{.base64_data = base64_data,
                            .id = {.i1 = i1, .i2 = i2},
                            .t = std::chrono::system_clock::from_time_t(ntp_timestamp),
                            .chunk = chunk,
                            .chunks = chunks});
        }
        return streams::publishers::of(std::move(packets));
      });

  return streams::publishers::merge(std::move(metadata), std::move(frames));
}

}  // namespace video
}  // namespace rtm
