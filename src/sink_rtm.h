#pragma once

#include "rtmclient.h"
#include "sink.h"

namespace rtm {
namespace video {

struct rtm_sink : public rtm::video::sink {
 public:
  rtm_sink(std::shared_ptr<rtm::publisher> client,
           const std::string &rtm_channel);

  void on_metadata(const metadata &m) override;
  void on_frame(const encoded_frame &f) override;

 private:
  std::shared_ptr<rtm::publisher> _client;
  std::string _frames_channel;
  std::string _metadata_channel;
  uint64_t _seq_id{0};
};

}  // namespace video
}  // namespace rtm