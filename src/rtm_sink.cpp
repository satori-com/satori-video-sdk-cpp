#include <iostream>

#include "data.h"
#include "satori_video.h"
#include "streams/streams.h"
#include "video_streams.h"

namespace satori {
namespace video {

namespace {

struct rtm_sink_impl : streams::subscriber<encoded_packet>, boost::static_visitor<void> {
  rtm_sink_impl(const std::shared_ptr<rtm::publisher> &client,
                boost::asio::io_service &io_service, const std::string &rtm_channel)
      : _client(client),
        _io_service(io_service),
        _frames_channel(rtm_channel),
        _metadata_channel(rtm_channel + metadata_channel_suffix) {}

  void on_next(encoded_packet &&packet) override {
    boost::apply_visitor(*this, packet);
    _src->request(1);
  }

  void on_error(std::error_condition ec) override { ABORT() << ec.message(); }

  void on_complete() override { delete this; }

  void on_subscribe(streams::subscription &s) override {
    _src = &s;
    _src->request(1);
  }

  void operator()(const encoded_metadata &m) {
    cbor_item_t *packet = m.to_network().to_cbor();
    CHECK_EQ(cbor_refcount(packet), 0);

    _io_service.post([ client = _client, channel = _metadata_channel, packet ]() {
      client->publish(channel, packet, nullptr);
    });
  }

  void operator()(const encoded_frame &f) {
    std::vector<network_frame> network_frames =
        f.to_network(std::chrono::system_clock::now());

    for (const network_frame &nf : network_frames) {
      cbor_item_t *packet = nf.to_cbor();
      CHECK_EQ(cbor_refcount(packet), 0);
      _io_service.post([ client = _client, channel = _frames_channel, packet ]() {
        client->publish(channel, packet, nullptr);
      });
    }

    _frames_counter++;
    if (_frames_counter % 100 == 0) {
      LOG(INFO) << "published " << _frames_counter << " frames to " << _frames_channel;
    }
  }

  const std::shared_ptr<rtm::publisher> _client;
  boost::asio::io_service &_io_service;
  const std::string _frames_channel;
  const std::string _metadata_channel;
  streams::subscription *_src;
  uint64_t _frames_counter{0};
};
}  // namespace

streams::subscriber<encoded_packet> &rtm_sink(
    const std::shared_ptr<rtm::publisher> &client, boost::asio::io_service &io_service,
    const std::string &rtm_channel) {
  return *(new rtm_sink_impl(client, io_service, rtm_channel));
}

}  // namespace video
}  // namespace satori
