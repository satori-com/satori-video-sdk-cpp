#include "source_rtm.h"
#include "base64.h"
#include "librtmvideo/rtmvideo.h"

namespace rtm {
namespace video {

rtm_source::rtm_source(std::shared_ptr<rtm::subscriber> subscriber,
                       const std::string &channel_name)
    : _subscriber(subscriber),
      _frames_channel(channel_name),
      _metadata_channel(channel_name + metadata_channel_suffix) {}

rtm_source::~rtm_source() {}

int rtm_source::init() { return 0; }

void rtm_source::start() {
  subscription_options metadata_options;
  metadata_options.history.count = 1;
  _subscriber->subscribe_channel(_metadata_channel, _metadata_subscription,
                                 *this, &metadata_options);
  _subscriber->subscribe_channel(_frames_channel, _frames_subscription, *this);
}

void rtm_source::on_data(const subscription &sub,
                         const rapidjson::Value &value) {
  if (&sub == &_metadata_subscription) {
    on_metadata(value);
  } else if (&sub == &_frames_subscription) {
    on_frame_data(value);
  } else {
    BOOST_ASSERT_MSG(false, "Unknown subscription");
  }
}

void rtm_source::on_metadata(const rapidjson::Value &msg) {
  std::string codec_data =
      msg.HasMember("codecData") ? decode64(msg["codecData"].GetString()) : "";
  std::string codec_name = msg["codecName"].GetString();

  source::foreach_sink([&codec_data, &codec_name](auto s) {
    s->on_metadata({.codec_name = codec_name, .codec_data = codec_data});
  });
}

void rtm_source::on_frame_data(const rapidjson::Value &msg) {
  auto t = msg["i"].GetArray();
  int64_t i1 = t[0].GetInt64();
  int64_t i2 = t[1].GetInt64();

  double ntp_timestamp = msg.HasMember("t") ? msg["t"].GetDouble() : 0;

  uint32_t chunk = 1, chunks = 1;
  if (msg.HasMember("c")) {
    chunk = msg["c"].GetInt();
    chunks = msg["l"].GetInt();
  }

  _aggregator.send_frame(
      {.base64_data = msg["d"].GetString(),
       .id = {i1, i2},
       .t = std::chrono::system_clock::from_time_t(ntp_timestamp),
       .chunk = chunk,
       .chunks = chunks});

  if (_aggregator.ready()) {
    const std::string data = decode64(_aggregator.get_data());
    const frame_id id = _aggregator.get_id();
    source::foreach_sink(
        [&id, &data](auto s) { s->on_frame({.data = data, .id = id}); });
  }
}

}  // namespace video
}  // namespace rtm
