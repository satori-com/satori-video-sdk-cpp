#include "rtm_streams.h"
#include "video_streams.h"

namespace rtm {
namespace video {

network_packet parse_network_metadata(rapidjson::Value &&msg) {
  const std::string name = msg["codecName"].GetString();
  const std::string base64_data =
      msg.HasMember("codecData") ? msg["codecData"].GetString() : "";

  return network_metadata{.codec_name = name, .base64_data = base64_data};
}

network_packet parse_network_frame(rapidjson::Value &&msg) {
  auto t = msg["i"].GetArray();
  int64_t i1 = t[0].GetInt64();
  int64_t i2 = t[1].GetInt64();

  std::chrono::system_clock::time_point timestamp;
  if (msg.HasMember("t")) {
    std::chrono::duration<double, std::nano> double_duration(msg["t"].GetDouble());
    std::chrono::system_clock::duration normal_duration =
        std::chrono::duration_cast<std::chrono::system_clock::duration>(double_duration);
    timestamp = std::chrono::system_clock::time_point{normal_duration};
  } else {
    LOG_S(WARNING) << "network frame packet doesn't have timestamp";
    timestamp = std::chrono::system_clock::now();
  }

  uint32_t chunk = 1, chunks = 1;
  if (msg.HasMember("c")) {
    chunk = msg["c"].GetInt();
    chunks = msg["l"].GetInt();
  }

  return network_frame{.base64_data = msg["d"].GetString(),
                       .id = {i1, i2},
                       .t = timestamp,
                       .chunk = chunk,
                       .chunks = chunks};
}

streams::publisher<network_packet> rtm_source(std::shared_ptr<rtm::subscriber> client,
                                              const std::string &channel_name) {
  subscription_options metadata_options;
  metadata_options.history.count = 1;

  streams::publisher<network_packet> metadata =
      streams::rtm::json_channel(client, channel_name + metadata_channel_suffix,
                                 metadata_options)
      >> streams::map(&parse_network_metadata) >> streams::take(1);

  streams::publisher<network_packet> frames =
      streams::rtm::json_channel(client, channel_name, {})
      >> streams::map(&parse_network_frame);

  return streams::publishers::merge(std::move(metadata), std::move(frames));
};

}  // namespace video
}  // namespace rtm
