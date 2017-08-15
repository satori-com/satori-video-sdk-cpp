#include <rapidjson/document.h>
#include <cstring>
#include <functional>
#include <gsl/gsl>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "source_replay.h"

extern "C" {
#include <libavutil/error.h>
}

namespace rtm {
namespace video {

replay_source::replay_source(const std::string &filename, bool synchronous)
    : _synchronous(synchronous),
      _frames_file(filename),
      _metadata_file(filename + ".metadata") {}

int replay_source::init() { return 0; }

void replay_source::send_metadata() {
  rapidjson::Document metadata_json;
  std::string line;
  std::getline(_metadata_file, line);
  metadata_json.Parse<0>(line.c_str()).HasParseError();
  assert(metadata_json.IsObject());

  const std::string name = metadata_json["codecName"].GetString();
  const std::string base64_data = metadata_json.HasMember("codecData")
                                      ? metadata_json["codecData"].GetString()
                                      : "";

  source::foreach_sink([&name, &base64_data](auto s) {
    s->on_metadata({.codec_name = name, .base64_data = base64_data});
  });
}

void replay_source::start() {
  double last_time = -1.0;
  send_metadata();

  for (std::string line; std::getline(_frames_file, line);) {
    rapidjson::Document data;
    data.Parse<0>(line.c_str()).HasParseError();
    assert(data.IsObject());

    if (_synchronous) {
      source::foreach_sink([&data](auto s) {
        while (!s->empty())
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
      });
    } else if (last_time != -1.0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(
          (int)((data["timestamp"].GetDouble() - last_time) * 1000)));
    }

    last_time = data["timestamp"].GetDouble();

    for (const auto &msg : data["messages"].GetArray()) {
      const std::string base64_data = msg["d"].GetString();
      const auto t = msg["i"].GetArray();
      const uint64_t i1 = t[0].GetUint64();
      const uint64_t i2 = t[1].GetUint64();

      const double ntp_timestamp =
          msg.HasMember("t") ? msg["t"].GetDouble() : 0;

      uint32_t chunk = 1, chunks = 1;
      if (msg.HasMember("c")) {
        chunk = msg["c"].GetUint();
        chunks = msg["l"].GetUint();
      }

      source::foreach_sink([&base64_data, &t, i1, i2, &ntp_timestamp, chunk,
                            chunks](auto s) {
        s->on_frame({.base64_data = base64_data,
                     .id = std::make_pair(i1, i2),
                     .t = std::chrono::system_clock::from_time_t(ntp_timestamp),
                     .chunk = chunk,
                     .chunks = chunks});
      });
    }
  }
}

}  // namespace video
}  // namespace rtm
