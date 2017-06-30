#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include "video_source_impl.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

namespace {
void print_av_error(const char *msg, int code) {
  char av_error[AV_ERROR_MAX_STRING_SIZE];
  std::cerr << msg
            << ", code: " << code
            << ", error: \"" << av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, code) << "\"\n";
}
}

struct file_video_source : public video_source {
 public:
  file_video_source(const char *filename, int is_replayed)
      : _filename(filename),
        _is_replayed(is_replayed) {}

  ~file_video_source() {
    if(_dec_ctx) avcodec_free_context(&_dec_ctx);
    if(_fmt_ctx) avformat_close_input(&_fmt_ctx);
  }

  int init() {
    int ret = 0;

    std::cout << "*** Opening file " << _filename << "\n";
    if((ret = avformat_open_input(&_fmt_ctx, _filename.c_str(), nullptr, nullptr)) < 0) {
      std::string error_msg = "*** Could not open file " + _filename;
      print_av_error(error_msg.c_str(), ret);
      return ret;
    }
    std::cout << "*** File " << _filename << " is open\n";

    std::cout << "*** Looking for stream info...\n";
    if((ret = avformat_find_stream_info(_fmt_ctx, nullptr)) < 0) {
      print_av_error("*** Could not find stream information", ret);
      return ret;
    }
    std::cout << "*** Stream info found\n";

    std::cout << "*** Number of streams " << _fmt_ctx->nb_streams << "\n";

    std::cout << "*** Looking for best stream...\n";
    if((ret = av_find_best_stream(_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &_dec, 0)) < 0) {
      print_av_error("*** Could not find video stream", ret);
      return ret;
    }
    std::cout << "*** Best stream found\n";

    _stream_idx = ret;
    _stream = _fmt_ctx->streams[_stream_idx];

    std::cout << "*** Allocating codec context...\n";
    _dec_ctx = avcodec_alloc_context3(_dec);
    if(!_dec_ctx) {
      std::cerr << "*** Failed to allocate codec context\n";
      return -1;
    }
    std::cout << "*** Codec context is allocated\n";

    std::cout << "*** Copying codec parameters to codec context...\n";
    if((ret = avcodec_parameters_to_context(_dec_ctx, _stream->codecpar)) < 0) {
      print_av_error("*** Failed to copy codec parameters to codec context", ret);
      return ret;
    }
    std::cout << "*** Codec parameters were copied to codec context\n";

    std::cout << "*** Opening video codec...\n";
    AVDictionary *opts = nullptr;
    if((ret = avcodec_open2(_dec_ctx, _dec, &opts)) < 0) {
      print_av_error("*** Failed to open video codec", ret);
      return ret;
    }
    std::cout << "*** Video codec is open\n";

    return 0;
  }

  int next_packet(uint8_t **output) {
    while(true) {
      int ret = av_read_frame(_fmt_ctx, &_pkt);
      if(ret < 0) {
        if(ret == AVERROR_EOF && _is_replayed) {
          av_seek_frame(_fmt_ctx, _stream_idx, _fmt_ctx->start_time, AVSEEK_FLAG_BACKWARD);
          continue;
        } else {
          print_av_error("*** Failed to read frame", ret);
          *output = nullptr;
          return -1;
        }
      }

      if(_pkt.stream_index == _stream_idx) {
        *output = new uint8_t[_pkt.size];
        std::memcpy(*output, _pkt.data, _pkt.size);
        ret = _pkt.size;
        av_packet_unref(&_pkt);
        return ret;
      } else {
        av_packet_unref(&_pkt);
        continue;
      }
    }
  }

  char *codec_name() const { return (char *) _dec->name; }

  int codec_data(uint8_t **output) const {
    *output = new uint8_t[_dec_ctx->extradata_size];
    std::memcpy(*output, _dec_ctx->extradata, _dec_ctx->extradata_size);
    return _dec_ctx->extradata_size;
  }

  size_t number_of_packets() const { return _stream->nb_frames; }

  double fps() const {
    return (double) _stream->avg_frame_rate.num / (double) _stream->avg_frame_rate.den;
  }

 private:
  AVFormatContext *_fmt_ctx{nullptr};
  int _stream_idx{-1};
  AVStream *_stream{nullptr};
  AVCodec *_dec{nullptr}; // TODO: check if possible to destroy it
  AVCodecContext *_dec_ctx{nullptr};
  AVPacket _pkt{0};
  std::string _filename;
  bool _is_replayed{false};
};

video_source *video_source_file_new(const char *filename, int is_replayed) {
  video_source_init_library();

  std::unique_ptr<video_source> vs(new file_video_source(filename, static_cast<bool>(is_replayed)));
  std::cout << "*** Initializing file video source...\n";
  int err = vs->init();
  if(err) {
    std::cerr << "*** Error initializing file video source, error code " << err << "\n";
    return nullptr;
  }
  std::cout << "*** File video source was initialized\n";

  return vs.release();
}