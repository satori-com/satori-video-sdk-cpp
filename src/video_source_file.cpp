#include "video_source_file.h"
#include <cstring>
#include <gsl/gsl>
#include <iostream>
#include <memory>
#include <string>

extern "C" {
#include <libavutil/error.h>
}

namespace rtm {
namespace video {

file_source::file_source(const std::string &filename, bool is_replayed)
    : _filename(filename), _is_replayed(is_replayed) {}

file_source::~file_source() {
  if (_dec_ctx) avcodec_free_context(&_dec_ctx);
  if (_fmt_ctx) avformat_close_input(&_fmt_ctx);
}

int file_source::init() {
  int ret = 0;

  std::cout << "*** Opening file " << _filename << "\n";
  if ((ret = avformat_open_input(&_fmt_ctx, _filename.c_str(), nullptr,
                                 nullptr)) < 0) {
    std::string error_msg = "*** Could not open file " + _filename;
    print_av_error(error_msg.c_str(), ret);
    return ret;
  }
  std::cout << "*** File " << _filename << " is open\n";

  std::cout << "*** Looking for stream info...\n";
  if ((ret = avformat_find_stream_info(_fmt_ctx, nullptr)) < 0) {
    print_av_error("*** Could not find stream information", ret);
    return ret;
  }
  std::cout << "*** Stream info found\n";

  std::cout << "*** Number of streams " << _fmt_ctx->nb_streams << "\n";

  std::cout << "*** Looking for best stream...\n";
  if ((ret = av_find_best_stream(_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &_dec,
                                 0)) < 0) {
    print_av_error("*** Could not find video stream", ret);
    return ret;
  }
  std::cout << "*** Best stream found\n";

  _stream_idx = ret;
  _stream = _fmt_ctx->streams[_stream_idx];

  std::cout << "*** Allocating codec context...\n";
  _dec_ctx = avcodec_alloc_context3(_dec);
  if (!_dec_ctx) {
    std::cerr << "*** Failed to allocate codec context\n";
    return -1;
  }
  std::cout << "*** Codec context is allocated\n";

  std::cout << "*** Copying codec parameters to codec context...\n";
  if ((ret = avcodec_parameters_to_context(_dec_ctx, _stream->codecpar)) < 0) {
    print_av_error("*** Failed to copy codec parameters to codec context", ret);
    return ret;
  }
  std::cout << "*** Codec parameters were copied to codec context\n";

  std::cout << "*** Opening video codec...\n";
  AVDictionary *opts = nullptr;
  if ((ret = avcodec_open2(_dec_ctx, _dec, &opts)) < 0) {
    print_av_error("*** Failed to open video codec", ret);
    return ret;
  }
  std::cout << "*** Video codec is open\n";

  return 0;
}

void file_source::start() {
  timed_source::start(
      _dec->name,
      {_dec_ctx->extradata, _dec_ctx->extradata + _dec_ctx->extradata_size},
      std::chrono::milliseconds(
          static_cast<int>((1000.0 * (double)_stream->avg_frame_rate.den) /
                           (double)_stream->avg_frame_rate.num)),
      std::chrono::milliseconds(10000));
}

boost::optional<std::string> file_source::next_packet() {
  while (true) {
    int ret = av_read_frame(_fmt_ctx, &_pkt);
    if (ret < 0) {
      if (ret == AVERROR_EOF) {
        if (_is_replayed) {
          av_seek_frame(_fmt_ctx, _stream_idx, _fmt_ctx->start_time,
                        AVSEEK_FLAG_BACKWARD);
          continue;
        } else {
          stop_timers();
          return boost::none;
        }
      } else {
        print_av_error("*** Failed to read frame", ret);
        return boost::none;
      }
    }

    if (_pkt.stream_index == _stream_idx) {
      auto release = gsl::finally([this]() { av_packet_unref(&_pkt); });
      return std::string{_pkt.data, _pkt.data + _pkt.size};
    } else {
      av_packet_unref(&_pkt);
      continue;
    }
  }
}

}  // namespace video
}  // namespace rtm