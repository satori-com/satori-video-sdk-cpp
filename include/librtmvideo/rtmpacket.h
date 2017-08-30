#pragma once

#include <cbor.h>
#include <chrono>
#include <string>
#include <boost/variant.hpp>

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

}  // namespace video
}  // namespace rtm