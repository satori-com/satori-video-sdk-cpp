#pragma once

#include <fstream>
#include "flow_json_decoder.h"
#include "flow_rtm_aggregator.h"
#include "source_replay.h"
#include "timed_source.h"

extern "C" {
#include <libavformat/avformat.h>
}

namespace rtm {
namespace video {

struct replay_proxy : public source<metadata, encoded_frame>,
                      public sink<metadata, encoded_frame>,
                      public std::enable_shared_from_this<replay_proxy> {
 public:
  replay_proxy(const std::string &filename, bool synchronous);

  void on_frame(encoded_frame &&f) override;
  void on_metadata(metadata &&m) override;

  int init() override;
  void start() override;

  bool empty() override;

 private:
  replay_source _replay;
  std::shared_ptr<flow_json_decoder> _decoder;
  std::shared_ptr<flow_rtm_aggregator> _aggregator;
};

}  // namespace video
}  // namespace rtm
