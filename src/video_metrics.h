#pragma once

#include "data.h"
#include "streams/streams.h"

namespace satori {
namespace video {

streams::op<network_packet, network_packet> report_video_metrics(
    const std::string& channel_name);

}  // namespace video
}  // namespace satori
