#pragma once

#include "rtmclient.h"
#include "sink.h"

namespace rtm {
namespace video {

struct rtm_sink : public rtm::video::sink<metadata, encoded_frame> {
 public:
  rtm_sink(std::shared_ptr<rtm::publisher> client,
           const std::string &rtm_channel);

  void on_metadata(metadata &&m) override;
  void on_frame(encoded_frame &&f) override;
  bool empty() override;

 private:
  const std::shared_ptr<rtm::publisher> _client;
  const std::string _frames_channel;
  const std::string _metadata_channel;
  uint64_t _frames_counter{0};
};

}  // namespace video
}  // namespace rtm
