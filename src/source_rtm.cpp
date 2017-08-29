#include "video_streams.h"

namespace rtm {
namespace video {

struct rtm_impl : public subscription_callbacks {
  rtm_impl(std::shared_ptr<rtm::subscriber> subscriber, const std::string &channel_name)
      : _subscriber(subscriber),
        _frames_channel(channel_name),
        _metadata_channel(channel_name + metadata_channel_suffix) {
    subscription_options metadata_options;
    metadata_options.history.count = 1;
    _subscriber->subscribe_channel(_metadata_channel, _metadata_subscription, *this,
                                   &metadata_options);
    _subscriber->subscribe_channel(_frames_channel, _frames_subscription, *this);
  }

  void start(streams::observer<network_packet> &s) { _sink = &s; }

  void on_data(const subscription &sub, rapidjson::Value &&value) {
    if (&sub == &_metadata_subscription) {
      on_metadata(value);
    } else if (&sub == &_frames_subscription) {
      on_frame_data(value);
    } else {
      BOOST_ASSERT_MSG(false, "Unknown subscription");
    }
  }

  void on_metadata(const rapidjson::Value &msg) {
    const std::string name = msg["codecName"].GetString();
    const std::string base64_data =
        msg.HasMember("codecData") ? msg["codecData"].GetString() : "";

    _sink->on_next(network_metadata{.codec_name = name, .base64_data = base64_data});
  }

  void on_frame_data(const rapidjson::Value &msg) {
    auto t = msg["i"].GetArray();
    int64_t i1 = t[0].GetInt64();
    int64_t i2 = t[1].GetInt64();

    double ntp_timestamp = msg.HasMember("t") ? msg["t"].GetDouble() : 0;

    uint32_t chunk = 1, chunks = 1;
    if (msg.HasMember("c")) {
      chunk = msg["c"].GetInt();
      chunks = msg["l"].GetInt();
    }

    auto frame = network_frame{.base64_data = msg["d"].GetString(),
                               .id = {i1, i2},
                               .t = std::chrono::system_clock::from_time_t(ntp_timestamp),
                               .chunk = chunk,
                               .chunks = chunks};

    _sink->on_next(frame);
  }

  const std::shared_ptr<rtm::subscriber> _subscriber;
  streams::observer<network_packet> *_sink;
  const std::string _metadata_channel;
  const std::string _frames_channel;
  rtm::subscription _metadata_subscription;
  rtm::subscription _frames_subscription;
};

streams::publisher<network_packet> rtm_source(std::shared_ptr<rtm::subscriber> client,
                                              const std::string &channel_name) {
  return streams::generators<network_packet>::async(
      [client, channel_name](streams::observer<network_packet> &o) {
        auto impl = new rtm_impl(client, channel_name);
        impl->start(o);
      });
};

}  // namespace video
}  // namespace rtm
