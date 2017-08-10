#pragma once

#include <fstream>
#include "timed_source.h"

extern "C" {
#include <libavformat/avformat.h>
}

namespace rtm {
namespace video {

struct replay_source : public source<rapidjson::Value, rapidjson::Value> {
 public:
  replay_source(const std::string &filename, bool synchronous);

  int init() override;
  void start() override;

 private:
  void send_metadata();

  std::string _filename;
  std::ifstream _framedata, _metadata;
  bool _synchronous{false};
};

}  // namespace video
}  // namespace rtm
