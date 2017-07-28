#pragma once

#include "source.h"

extern "C" {
#include <libavformat/avformat.h>
}

namespace rtm {
namespace video {

struct file_source : public timed_source {
 public:
  file_source(const std::string &filename, bool is_replayed);
  ~file_source();

  int init() override;
  void start() override;

 private:
  boost::optional<std::string> next_packet() override;

 private:
  AVFormatContext *_fmt_ctx{nullptr};
  int _stream_idx{-1};
  AVStream *_stream{nullptr};
  AVCodec *_dec{nullptr};  // TODO: check if possible to destroy it
  AVCodecContext *_dec_ctx{nullptr};
  AVPacket _pkt{0};
  std::string _filename;
  bool _is_replayed{false};
};

}  // namespace video
}  // namespace rtm