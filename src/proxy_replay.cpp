#include <rapidjson/document.h>
#include <cstring>
#include <functional>
#include <gsl/gsl>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "proxy_replay.h"

extern "C" {
#include <libavutil/error.h>
}

namespace rtm {
namespace video {
replay_proxy::replay_proxy(const std::string &filename, bool synchronous)
    : _replay(filename, synchronous) {
  _decoder.reset(new flow_json_decoder());
  _aggregator.reset(new flow_rtm_aggregator());
}

int replay_proxy::init() { return _decoder->init() + _replay.init() + _aggregator->init(); }

bool replay_proxy::empty() {
  bool empty = true;
  source::foreach_sink([&empty](auto s) { empty = empty & s->empty(); });
  return empty;
}

void replay_proxy::on_frame(encoded_frame &&f) {
  source::foreach_sink([&f](auto s) { s->on_frame(std::move(f)); });
}

void replay_proxy::on_metadata(metadata &&m) {
  source::foreach_sink([&m](auto s) { s->on_metadata(std::move(m)); });
}

void replay_proxy::start() {
  _aggregator->subscribe(shared_from_this());
  _decoder->subscribe(_aggregator);
  _replay.subscribe(_decoder);
  _replay.start();
}
}  // namespace video
}  // namespace rtm
