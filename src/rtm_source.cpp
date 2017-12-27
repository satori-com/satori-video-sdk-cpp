#include "rtm_streams.h"
#include "video_streams.h"

namespace satori {
namespace video {

streams::publisher<network_packet> rtm_source(
    const std::shared_ptr<rtm::subscriber> &client, const std::string &channel_name) {
  rtm::subscription_options metadata_options;
  metadata_options.history.count = 1;

  streams::publisher<network_packet> metadata =
      rtm::channel(client, channel_name + metadata_channel_suffix, metadata_options)
      >> streams::map([](rtm::channel_data &&data) {
          return network_packet{parse_network_metadata(data.payload)};
        });

  streams::publisher<network_packet> frames =
      rtm::channel(client, channel_name, {})
      >> streams::map([](rtm::channel_data &&data) {
          network_frame f = parse_network_frame(data.payload);
          f.arrival_time = data.arrival_time;
          return network_packet{f};
        });

  return streams::publishers::merge(std::move(metadata), std::move(frames));
}

}  // namespace video
}  // namespace satori
