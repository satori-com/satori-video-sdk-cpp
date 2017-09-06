#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

#include "librtmvideo/data.h"
#include "sink.h"
#include "source.h"

namespace rtm {
namespace video {

// TODO: use streams API
struct vp9_transcoder : public sink<image_metadata, image_frame>,
                        public source<encoded_metadata, encoded_frame> {
 public:
  vp9_transcoder(uint8_t lag_in_frames);
  ~vp9_transcoder();

  int init() override;
  void start() override;

  void on_metadata(image_metadata &&) override;
  void on_frame(image_frame &&f) override;
  bool empty() override;

 private:
  // https://www.webmproject.org/docs/encoder-parameters/
  // The --lag-in-frames parameter defines an upper limit on the number of frames into the
  // future that the encoder can look.
  const uint8_t _lag_in_frames;
  bool _initialized{false};
  const AVCodecID _encoder_id{AV_CODEC_ID_VP9};
  std::shared_ptr<AVCodecContext> _encoder_context{nullptr};
  std::shared_ptr<AVFrame> _tmp_frame{nullptr};  // for pixel format conversion
  std::shared_ptr<AVFrame> _frame{nullptr};
  std::shared_ptr<SwsContext> _sws_context{nullptr};
};

}  // namespace video
}  // namespace rtm