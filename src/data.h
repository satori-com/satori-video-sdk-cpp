#pragma once

#include <cbor.h>
#include <boost/optional.hpp>
#include <boost/variant.hpp>
#include <chrono>
#include <ostream>
#include <string>
#include <vector>

#include "satori_video.h"
#include "satorivideo/video_bot.h"

namespace satori {
namespace video {

inline bool operator==(const frame_id &lhs, const frame_id &rhs) {
  return lhs.i1 == rhs.i1 && lhs.i2 == rhs.i2;
}

inline std::ostream &operator<<(std::ostream &os, const frame_id &id) {
  os << "[" << id.i1 << ", " << id.i2 << "]";
  return os;
}

inline bool operator!=(const frame_id &lhs, const frame_id &rhs) { return !(lhs == rhs); }

static constexpr size_t max_payload_size = 65000;

// network representation of codec parameters, e.g. in binary data
// is converted into base64, because RTM supports only text/json data
struct network_metadata {
  ~network_metadata() {
    if (additional_data != nullptr) {
      cbor_decref(&additional_data);
    }
  }

  std::string codec_name;
  std::string base64_data;
  cbor_item_t *additional_data{nullptr};

  cbor_item_t *to_cbor() const;
};

// network representation of encoded video frame, e.g. binary data
// is converted into base64, because RTM supports only text/json data
struct network_frame {
  std::string base64_data;
  frame_id id{0, 0};
  std::chrono::high_resolution_clock::time_point t;
  uint32_t chunk{1};
  uint32_t chunks{1};
  bool key_frame{false};

  std::chrono::high_resolution_clock::time_point arrival_time;
  cbor_item_t *to_cbor() const;
};

// algebraic type to support flow of network data using streams API
using network_packet = boost::variant<network_metadata, network_frame>;

network_metadata parse_network_metadata(cbor_item_t *item);
network_frame parse_network_frame(cbor_item_t *item);

// Used to tell not to downscale original video stream
constexpr int16_t original_image_width = -1;
constexpr int16_t original_image_height = -1;

// image size
struct image_size {
  int16_t width;
  int16_t height;
};

// codec parameters to decode encoded frames
struct encoded_metadata {
  ~encoded_metadata() {
    if (additional_data != nullptr) {
      cbor_decref(&additional_data);
    }
  }

  std::string codec_name;
  std::string codec_data;
  boost::optional<struct image_size> image_size;

  cbor_item_t *additional_data{nullptr};

  network_metadata to_network() const;
};

// encoded frame
struct encoded_frame {
  std::string data;
  frame_id id;

  // PTS time
  std::chrono::high_resolution_clock::time_point timestamp;

  bool key_frame{false};

  // time when frame came from network
  std::chrono::high_resolution_clock::time_point arrival_time;

  std::vector<network_frame> to_network() const;
};

// algebraic type to support flow of encoded data using streams API
using encoded_packet = boost::variant<encoded_metadata, encoded_frame>;

// TODO: may contain some data like FPS, etc.
struct owned_image_metadata {};

// If an image uses packed pixel format like packed RGB or packed YUV,
// then it has only a single plane, e.g. all it's data is within plane_data[0].
// If an image uses planar pixel format like planar YUV or HSV,
// then every component is stored as a separate array (e.g. separate plane),
// for example, for YUV  Y is plane_data[0], U is plane_data[1] and V is
// plane_data[2]. A stride is a plane size with alignment.
struct owned_image_frame {
  frame_id id;

  image_pixel_format pixel_format;
  uint16_t width;
  uint16_t height;

  // image capture time
  std::chrono::high_resolution_clock::time_point timestamp;

  std::string plane_data[max_image_planes];
  uint32_t plane_strides[max_image_planes];
};

// algebraic type to support flow of image data using streams API
using owned_image_packet = boost::variant<owned_image_metadata, owned_image_frame>;

}  // namespace video
}  // namespace satori

std::ostream &operator<<(std::ostream &out,
                         const satori::video::network_metadata &metadata);

std::ostream &operator<<(std::ostream &out,
                         const satori::video::encoded_metadata &metadata);
