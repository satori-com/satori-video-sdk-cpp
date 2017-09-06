#pragma once

#include <boost/variant.hpp>
#include <cbor.h>
#include <chrono>
#include <string>
#include <vector>

#include "rtmvideo.h"

namespace rtm {
namespace video {

static constexpr size_t max_payload_size = 65000;

using frame_id = std::pair<uint64_t, uint64_t>;

struct network_metadata {
  std::string codec_name;
  std::string base64_data;

  cbor_item_t *to_cbor() const;
};

struct network_frame {
  std::string base64_data;
  frame_id id{0, 0};
  std::chrono::system_clock::time_point t;
  uint32_t chunk{1};
  uint32_t chunks{1};

  cbor_item_t *to_cbor() const;
};

using network_packet = boost::variant<network_metadata, network_frame>;

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

using encoded_packet = boost::variant<metadata, encoded_frame>;

struct image_metadata {};

// for unencoded video frames
struct image_frame {
  frame_id id;

  image_pixel_format pixel_format;
  uint16_t width;
  uint16_t height;

  std::string plane_data[MAX_IMAGE_PLANES];
  uint32_t plane_strides[MAX_IMAGE_PLANES];
};

}
}