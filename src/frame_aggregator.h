#pragma once

#include <string>

#include "librtmvideo/rtmpacket.h"

namespace rtm {
namespace video {

struct frame_aggregator {
 public:
  frame_aggregator();
  ~frame_aggregator();

  void send_frame(const network_frame &f);
  bool ready() const;
  std::string get_data() const;
  frame_id get_id() const;

 private:
  void reset();

 private:
  bool _ready;
  uint8_t _expected_chunk;
  uint8_t _chunks;
  frame_id _frame_id;
  std::string _aggregated_data;
};

}  // namespace video
}  // namespace rtm