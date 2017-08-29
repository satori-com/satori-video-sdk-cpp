#pragma once

#include "librtmvideo/data.h"
#include "rtmclient.h"
#include "video_streams.h"

namespace rtm {
namespace video {

struct rtm_sink : public streams::subscriber<encoded_packet> {
 public:
  rtm_sink(std::shared_ptr<rtm::publisher> client,
           const std::string &rtm_channel);
  void on_next(encoded_packet &&t) override;
  void on_error(std::error_condition ec) override;
  void on_complete() override;
  void on_subscribe(streams::subscription &s) override;

 private:
  void on_metadata(const encoded_metadata &m);
  void on_image_frame(const encoded_frame &f);

  const std::shared_ptr<rtm::publisher> _client;
  const std::string _frames_channel;
  const std::string _metadata_channel;
  streams::subscription *_src;
  uint64_t _frames_counter{0};
};

}  // namespace video
}  // namespace rtm
