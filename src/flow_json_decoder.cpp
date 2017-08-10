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

network_metadata flow_json_decoder::decode_metadata_frame(
    const rapidjson::Value &msg) {
  std::string codec_data =
      msg.HasMember("codecData") ? msg["codecData"].GetString() : "";
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
  network_metadata mm = decode_metadata_frame(m);
  source::foreach_sink([&mm](auto s) { s->on_metadata(std::move(mm)); });
}

void flow_json_decoder::on_frame(rapidjson::Value &&f) {
  network_frame ff = decode_network_frame(f);
  source::foreach_sink([&ff](auto s) { s->on_frame(std::move(ff)); });
}

bool flow_json_decoder::empty() {
  bool empty = true;
  source::foreach_sink([&empty](auto s) { empty = empty & s->empty(); });
  return empty;
}

}  // namespace video
}  // namespace rtm
