#pragma once

#include <fstream>
#include "source.h"

namespace rtm {
namespace video {

struct replay_source : public source<network_metadata, network_frame> {
 public:
  replay_source(const std::string &filename, bool synchronous);

  int init() override;
  void start() override;

 private:
  void send_metadata();

  std::ifstream _frames_file;
  std::ifstream _metadata_file;
  const bool _synchronous;
};

}  // namespace video
}  // namespace rtm
