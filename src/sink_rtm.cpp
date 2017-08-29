#include <iostream>

#include "librtmvideo/rtmvideo.h"
#include "sink_rtm.h"

namespace rtm {
namespace video {

rtm_sink::rtm_sink(std::shared_ptr<rtm::publisher> client,
                   const std::string &rtm_channel)
    : _client(client),
      _frames_channel(rtm_channel),
      _metadata_channel(rtm_channel + metadata_channel_suffix) {}

void rtm_sink::on_next(encoded_packet &&packet) {
  if (const encoded_metadata *m = boost::get<encoded_metadata>(&packet)) {
    on_metadata(*m);
  } else if (const encoded_frame *f = boost::get<encoded_frame>(&packet)) {
    on_image_frame(*f);
  } else {
    BOOST_ASSERT_MSG(false, "Bad variant");
  }
  _src->request(1);
}

void rtm_sink::on_error(std::error_condition ec) {
  std::cerr << "ERROR: " << ec.message() << "\n";
  exit(1);
}

void rtm_sink::on_complete() { delete this; }

void rtm_sink::on_subscribe(streams::subscription &s) {
  _src = &s;
  _src->request(1);
}

void rtm_sink::on_metadata(const encoded_metadata &m) {
  cbor_item_t *packet = m.to_network().to_cbor();
  _client->publish(_metadata_channel, packet, nullptr);
  cbor_decref(&packet);
}

void rtm_sink::on_image_frame(const encoded_frame &f) {
  std::vector<network_frame> network_frames =
      f.to_network(std::chrono::system_clock::now());

  for (const network_frame &nf : network_frames) {
    cbor_item_t *packet = nf.to_cbor();
    _client->publish(_frames_channel, packet, nullptr);
    cbor_decref(&packet);
  }

  _frames_counter++;
  if (_frames_counter % 100 == 0) {
    std::cout << "Published " << _frames_counter << " frames\n";
  }
}

}  // namespace video
}  // namespace rtm
