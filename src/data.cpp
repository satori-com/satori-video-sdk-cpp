#include <cmath>

#include "base64.h"
#include "librtmvideo/data.h"

namespace rtm {
namespace video {

network_metadata metadata::to_network() const {
  network_metadata nm;

  nm.codec_name = codec_name;
  if (codec_data.size() > 0) {
    nm.base64_data = std::move(rtm::video::encode64(codec_data));
  }

  return nm;
}

std::vector<network_frame> encoded_frame::to_network(
    const frame_id &id,
    std::chrono::system_clock::time_point t) const {
  std::vector<network_frame> frames;

  std::string encoded = std::move(rtm::video::encode64(data));
  size_t chunks = std::ceil((double) encoded.length() / max_payload_size);

  for (size_t i = 0; i < chunks; i++) {
    frames.push_back({
        .base64_data = encoded.substr(i * max_payload_size, max_payload_size),
        .id = id,
        .t = t,
        .chunk = static_cast<uint32_t>(i + 1),
        .chunks = static_cast<uint32_t>(chunks)
    });
  }

  return frames;
}

}
}