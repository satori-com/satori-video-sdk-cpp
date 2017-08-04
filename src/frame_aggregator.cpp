#include "frame_aggregator.h"

namespace rtm {
namespace video {

frame_aggregator::frame_aggregator() { reset(); }

frame_aggregator::~frame_aggregator() {}

void frame_aggregator::send_frame(const network_frame &f) {
  if (_frame_id != f.id) reset();

  if (_ready) return;

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
    _ready = true;
  }
}

bool frame_aggregator::ready() const { return _ready; }

std::string frame_aggregator::get_data() const { return _aggregated_data; }

frame_id frame_aggregator::get_id() const { return _frame_id; };

void frame_aggregator::reset() {
  _ready = false;
  _expected_chunk = 1;
  _chunks = 1;
  _frame_id = {-1, -1};
  _aggregated_data.clear();
}

}  // namespace video
}  // namespace rtm