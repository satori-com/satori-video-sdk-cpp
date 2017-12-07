#include "rtm_streams.h"
#include "video_streams.h"

#include "cbor_tools.h"

namespace satori {
namespace video {

network_packet parse_network_metadata(cbor_item_t *item) {
  cbor_incref(item);
  auto decref = gsl::finally([&item]() { cbor_decref(&item); });
  auto msg = cbor::map(item);

  const std::string name = msg.get_str("codecName");
  const std::string base64_data = msg.get_str("codecData");

  return network_metadata{name, base64_data};
}

network_packet parse_network_frame(cbor_item_t *item) {
  CHECK_EQ(1, cbor_refcount(item));
  auto decref = gsl::finally([&item]() { cbor_decref(&item); });
  auto msg = cbor::map(item);

  auto id = msg.get("i");
  int64_t i1 = cbor::get_int64(cbor_array_handle(id)[0]);
  int64_t i2 = cbor::get_int64(cbor_array_handle(id)[1]);

  std::chrono::system_clock::time_point timestamp;
  const cbor_item_t *t = msg.get("t");
  if (t != nullptr) {
    std::chrono::duration<double, std::nano> double_duration(cbor::get_double(t));
    std::chrono::system_clock::duration normal_duration =
        std::chrono::duration_cast<std::chrono::system_clock::duration>(double_duration);
    timestamp = std::chrono::system_clock::time_point{normal_duration};
  } else {
    LOG(WARNING) << "network frame packet doesn't have timestamp";
    timestamp = std::chrono::system_clock::now();
  }

  uint32_t chunk = 1, chunks = 1;
  const cbor_item_t *c = msg.get("c");
  if (c != nullptr) {
    chunk = cbor_get_uint32(c);
    chunks = cbor_get_uint32(msg.get("l"));
  }

  const cbor_item_t *k = msg.get("k");
  bool key_frame = false;
  if (k != nullptr) {
    key_frame = cbor_is_bool(k) && cbor_ctrl_is_bool(k);
  }

  network_frame frame;
  frame.base64_data = msg.get_str("d");
  frame.id = {i1, i2};
  frame.t = timestamp;
  frame.chunk = chunk;
  frame.chunks = chunks;
  frame.key_frame = key_frame;

  return frame;
}

streams::publisher<network_packet> rtm_source(
    const std::shared_ptr<rtm::subscriber> &client, const std::string &channel_name) {
  rtm::subscription_options metadata_options;
  metadata_options.history.count = 1;

  streams::publisher<network_packet> metadata =
      rtm::cbor_channel(client, channel_name + metadata_channel_suffix, metadata_options)
      >> streams::map(&parse_network_metadata);

  streams::publisher<network_packet> frames =
      rtm::cbor_channel(client, channel_name, {}) >> streams::map(&parse_network_frame);

  return streams::publishers::merge(std::move(metadata), std::move(frames));
}

}  // namespace video
}  // namespace satori
