#pragma once

#include <memory>
#include <string>

#include "librtmvideo/data.h"
#include "sink.h"
#include "source.h"

namespace rtm {
namespace video {

struct flow_rtm_aggregator : public sink<network_metadata, network_frame>,
                             public source<encoded_metadata, encoded_frame>,
                             public std::enable_shared_from_this<flow_rtm_aggregator> {
 public:
  // TODO: use streams API instead
  flow_rtm_aggregator(
      std::unique_ptr<source<network_metadata, network_frame>> source);
  ~flow_rtm_aggregator();

  int init() override;
  void start() override;
  bool empty() override;

 private:
  void on_metadata(network_metadata &&m) override;
  void on_frame(network_frame &&f) override;
  void reset();

 private:
  const std::unique_ptr<source<network_metadata, network_frame>> _source;
  uint8_t _expected_chunk;
  uint8_t _chunks;
  frame_id _frame_id;
  std::string _aggregated_data;
};

}  // namespace video
}  // namespace rtm
