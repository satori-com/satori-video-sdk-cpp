#include <iostream>

#include "librtmvideo/rtmpacket.h"
#include "librtmvideo/rtmvideo.h"
#include "sink_rtm.h"

namespace rtm {
namespace video {

rtm_sink::rtm_sink(std::shared_ptr<rtm::publisher> client,
                   const std::string &rtm_channel)
    : _client(client),
      _frames_channel(rtm_channel),
      _metadata_channel(rtm_channel + metadata_channel_suffix) {}

void rtm_sink::on_metadata(const metadata &m) {
  cbor_item_t *packet = m.to_network().to_cbor();
  _client->publish(_metadata_channel, packet, nullptr);
  cbor_decref(&packet);
}

void rtm_sink::on_frame(const encoded_frame &f) {
  std::vector<network_frame> network_frames =
      f.to_network({_seq_id, _seq_id}, std::chrono::system_clock::now());

  for (const network_frame &nf : network_frames) {
    cbor_item_t *packet = nf.to_cbor();
    _client->publish(_frames_channel, packet, nullptr);
    cbor_decref(&packet);
  }

  _seq_id++;
  if (_seq_id % 100 == 0) {
    std::cout << "Published " << _seq_id << " frames\n";
  }
}

bool rtm_sink::empty() { return true; }

}  // namespace video
}  // namespace rtm
