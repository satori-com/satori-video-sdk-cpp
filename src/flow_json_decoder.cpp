#include <boost/assert.hpp>
#include <iostream>

#include "base64.h"
#include "flow_json_decoder.h"
#include "stopwatch.h"
#include "tele_impl.h"

namespace rtm {
namespace video {

int flow_json_decoder::init() { return 0; }

void flow_json_decoder::start() {}

metadata flow_json_decoder::decode_metadata_frame(const rapidjson::Value &msg) {
  std::string codec_data =
      msg.HasMember("codecData") ? decode64(msg["codecData"].GetString()) : "";
  std::string codec_name = msg["codecName"].GetString();
  return {codec_name, codec_data};
}

network_frame flow_json_decoder::decode_network_frame(
    const rapidjson::Value &msg) {
  auto t = msg["i"].GetArray();
  uint64_t i1 = t[0].GetUint64();
  uint64_t i2 = t[1].GetUint64();

  double ntp_timestamp = msg.HasMember("t") ? msg["t"].GetDouble() : 0;

  uint32_t chunk = 1, chunks = 1;
  if (msg.HasMember("c")) {
    chunk = msg["c"].GetUint();
    chunks = msg["l"].GetUint();
  }

  return {msg["d"].GetString(), std::make_pair(i1, i2),
          std::chrono::system_clock::from_time_t(ntp_timestamp), chunk, chunks};
}

void flow_json_decoder::on_metadata(rapidjson::Value &&m) {
  on_metadata(decode_metadata_frame(m));
}

void flow_json_decoder::on_frame(rapidjson::Value &&f) {
  on_frame(decode_network_frame(f));
}

void flow_json_decoder::on_metadata(metadata &&m) {
  source::foreach_sink([&m](auto s) { s->on_metadata(std::move(m)); });
}

bool flow_json_decoder::empty() {
  bool empty = true;
  source::foreach_sink([&empty](auto s) { empty = empty & s->empty(); });
  return empty;
}

void flow_json_decoder::process_frame_part(frame_id &id, const uint8_t *data,
                                           size_t length, uint32_t chunk,
                                           uint32_t chunks) {
  if (chunks == 1) {
    process_frame(id, data, length);
    return;
  }

  if (chunk == 1) {
    _chunk_buffer.clear();
  }
  _chunk_buffer.insert(_chunk_buffer.end(), data, data + length);
  if (chunk != chunks) return;

  process_frame(id, _chunk_buffer.data(), _chunk_buffer.size());
  _chunk_buffer.clear();
}

void flow_json_decoder::process_frame(frame_id &id, const uint8_t *data,
                                      size_t length) {
  std::string encoded{data, data + length};
  std::string decoded = rtm::video::decode64(encoded);
  source::foreach_sink([&decoded, &id](auto s) {
    s->on_frame(encoded_frame{.data = decoded, .id = id});
  });
}

void flow_json_decoder::on_frame(network_frame &&f) {
  process_frame_part(f.id, (const uint8_t *)f.base64_data.c_str(),
                     f.base64_data.size(), f.chunk, f.chunks);
}
}  // namespace video
}  // namespace rtm
