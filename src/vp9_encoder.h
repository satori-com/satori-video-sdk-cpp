#pragma once

#include "librtmvideo/data.h"
#include "streams.h"

namespace rtm {
namespace video {

streams::op<image_packet, encoded_packet> encode_vp9(uint8_t lag_in_frames);
}
}  // namespace rtm