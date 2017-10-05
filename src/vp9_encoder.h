#pragma once

#include "data.h"
#include "streams/streams.h"

namespace rtm {
namespace video {

streams::op<owned_image_packet, encoded_packet> encode_vp9(uint8_t lag_in_frames);
}
}  // namespace rtm
