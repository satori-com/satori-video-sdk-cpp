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
    : _filename(filename),
      _synchronous(synchronous),
      _framedata(filename),
      _metadata(filename + ".metadata") {}

int replay_source::init() { return 0; }

void replay_source::send_metadata() {
  rapidjson::Document metadata_json;
  std::string line;
  std::getline(_metadata, line);
  metadata_json.Parse<0>(line.c_str()).HasParseError();
  assert(metadata_json.IsObject());
  source::foreach_sink([&metadata_json](auto s) {
    s->on_metadata(std::move(metadata_json.GetObject()));
  });
}

void replay_source::start() {
  double last_time = -1.0;
  send_metadata();

  for (std::string line; std::getline(_framedata, line);) {
    rapidjson::Document data;
    data.Parse<0>(line.c_str()).HasParseError();
    assert(data.IsObject());
    if (_synchronous)
      source::foreach_sink([&data](auto s) {
        while (!s->empty())
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
      });
    else if (last_time != -1.0)
      std::this_thread::sleep_for(std::chrono::milliseconds(
          (int)((data["timestamp"].GetDouble() - last_time) * 1000)));
    last_time = data["timestamp"].GetDouble();
    for (auto &m : data["messages"].GetArray())
      source::foreach_sink([&m](auto s) { s->on_frame(std::move(m)); });
  }
}

}  // namespace video
}  // namespace rtm
