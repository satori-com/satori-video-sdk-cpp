#include "flow_rtm_aggregator.h"
#include "base64.h"

namespace rtm {
namespace video {

flow_rtm_aggregator::flow_rtm_aggregator() { reset(); }

flow_rtm_aggregator::~flow_rtm_aggregator() {}

int flow_rtm_aggregator::init() { return 0; }

void flow_rtm_aggregator::start(){};

void flow_rtm_aggregator::on_metadata(network_metadata &&m) {
  const std::string codec_data = decode64(m.base64_data);
  source::foreach_sink([&m, &codec_data](auto s) {
    s->on_metadata({.codec_name = m.codec_name, .codec_data = codec_data});
  });
}

void flow_rtm_aggregator::on_frame(network_frame &&f) {
  if (_frame_id != f.id) reset();

  if (_expected_chunk != f.chunk) {
    reset();
    return;
  }

  _expected_chunk++;
  if (f.chunk == 1) {
    _chunks = f.chunks;
    _frame_id = f.id;
  }
  _aggregated_data.append(f.base64_data);

  if (f.chunk == _chunks) {
    source::foreach_sink([this](auto s) {
      s->on_frame({.data = decode64(_aggregated_data), .id = _frame_id});
    });
    reset();
  }
}

void flow_rtm_aggregator::reset() {
  _expected_chunk = 1;
  _chunks = 1;
  _frame_id = {-1, -1};
  _aggregated_data.clear();
}

bool flow_rtm_aggregator::empty() {
  bool empty = true;
  source::foreach_sink([&empty](auto s) { empty = empty & s->empty(); });
  return empty;
}

}  // namespace video
}  // namespace rtm
