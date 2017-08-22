#pragma once

#include <vector>

#include "rtmpacket.h"
#include "rtmvideo.h"

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
  frame_id id;

  image_pixel_format pixel_format;
  uint16_t width;
  uint16_t height;

  const uint8_t *plane_data[MAX_IMAGE_PLANES];
  uint32_t plane_strides[MAX_IMAGE_PLANES];
};

}
}