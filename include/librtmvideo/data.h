#pragma once

#include <vector>

#include "rtmpacket.h"

namespace rtm {
namespace video {

struct metadata {
  std::string codec_name;
  std::string codec_data;

  network_metadata to_network() const;
};

// for encoded frames
struct encoded_frame {
  std::string data;
  frame_id id;

  std::vector<network_frame> to_network(
      std::chrono::system_clock::time_point t) const;
};

// for unencoded video frames
struct image_frame {
  std::string image_data;
  frame_id id;
  uint16_t width;
  uint16_t height;
  uint16_t linesize;
};

struct metadata_frame {
  std::string codec_name;
  std::string codec_data;
};

}
}