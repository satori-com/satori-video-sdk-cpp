#pragma once

#include "timed_source.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

namespace rtm {
namespace video {

struct camera_source : public timed_source {
 public:
  camera_source(const std::string &dimensions);
  ~camera_source();

  int init() override;
  void start() override;

 private:
  boost::optional<std::string> next_packet() override;
  void init_open_parameters(AVInputFormat **input_format,
                            AVDictionary **options);

 private:
  std::string _dimensions;
  AVFormatContext *_fmt_ctx{nullptr};
  int _stream_idx{-1};
  AVStream *_stream{nullptr};
  // rawvideo: uyvy422 yuyv422 nv12 0rgb bgr0
  AVPixelFormat _dec_pix_fmt{AV_PIX_FMT_BGR0};
  AVCodecID _dec_id{AV_CODEC_ID_RAWVIDEO};
  AVCodec *_dec{nullptr};  // TODO: deallocate?
  AVCodecContext *_dec_ctx{nullptr};
  AVPacket _dec_pkt{0};
  AVFrame *_dec_frame{nullptr};
  // mjpeg: yuvj420p yuvj422p yuvj444p
  // jpeg2000: rgb24 yuv444p gray yuv420p yuv422p yuv410p yuv411p
  AVPixelFormat _enc_pix_fmt{AV_PIX_FMT_YUVJ422P};
  AVCodecID _enc_id{AV_CODEC_ID_MJPEG};
  AVCodec *_enc{nullptr};  // TODO: deallocate?
  AVCodecContext *_enc_ctx{nullptr};
  AVPacket _enc_pkt{0};
  AVFrame *_enc_frame{nullptr};
  SwsContext *_sws_ctx{nullptr};
};

}  // namespace video
}  // namespace rtm