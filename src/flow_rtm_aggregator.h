#pragma once

#include <string>

#include "librtmvideo/rtmpacket.h"
#include "sink.h"
#include "source.h"

namespace rtm {
namespace video {

struct flow_rtm_aggregator : public sink<network_metadata, network_frame>,
                             public source<metadata, encoded_frame> {
 public:
  flow_rtm_aggregator();
  ~flow_rtm_aggregator();

  int init() override;
  void start() override;

  void on_metadata(network_metadata &&m) override;
  void on_frame(network_frame &&f) override;

 private:
  void reset();

 private:
  uint8_t _expected_chunk;
  uint8_t _chunks;
  frame_id _frame_id;
  std::string _aggregated_data;
};

}  // namespace video
}  // namespace rtm