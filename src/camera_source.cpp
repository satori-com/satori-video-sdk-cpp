#include <cstring>
#include <gsl/gsl>
#include <iostream>
#include <memory>
#include <string>

extern "C" {
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include "avutils.h"
#include "librtmvideo/base.h"
#include "streams/asio_streams.h"
#include "video_error.h"
#include "video_streams.h"

namespace rtm {
namespace video {

struct camera_source_impl {
  explicit camera_source_impl(const std::string &dimensions) : _dimensions(dimensions) {}

  ~camera_source_impl() {
    if (_dec_frame) av_frame_free(&_dec_frame);
    if (_enc_frame) av_frame_free(&_enc_frame);
    if (_sws_ctx) sws_freeContext(_sws_ctx);
    if (_dec_ctx) avcodec_free_context(&_dec_ctx);
    if (_fmt_ctx) avformat_close_input(&_fmt_ctx);
  }

  int init() {
    int ret = 0;
    AVInputFormat *input_format = nullptr;  // TODO: deallocate?
    AVDictionary *options = nullptr;
    init_open_parameters(&input_format, &options);

    std::cout << "*** Looking for decoder \"" << avcodec_get_name(_dec_id) << "\"...\n";
    _dec = avcodec_find_decoder(_dec_id);
    if (!_dec) {
      std::cerr << "*** Decoder was not found\n";
      return -1;
    }
    std::cout << "*** Decoder was found\n";

    std::cout << "*** Setting codec to format context...\n";
    _fmt_ctx = avformat_alloc_context();
    av_format_set_video_codec(_fmt_ctx, _dec);
    std::cout << "*** Codec was set to format context\n";

    std::cout << "*** Opening camera...\n";
    if ((ret = avformat_open_input(&_fmt_ctx, "0", input_format, &options)) < 0) {
      std::cerr << "*** Could not open camera:" << avutils::error_msg(ret) << "\n";
      return ret;
    }
    std::cout << "*** Camera is open\n";

    _stream_idx = avutils::find_best_video_stream(_fmt_ctx, &_dec);
    if (_stream_idx < 0) {
      return ret;
    }
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
      std::cerr << "*** Failed to copy codec parameters to codec context:"
                << avutils::error_msg(ret) << "\n";
      return ret;
    }
    std::cout << "*** Codec parameters were copied to codec context\n";

    std::cout << "*** Opening video codec...\n";
    if ((ret = avcodec_open2(_dec_ctx, _dec, nullptr)) < 0) {
      std::cerr << "*** Failed to open video codec:" << avutils::error_msg(ret) << "\n";
      return ret;
    }
    std::cout << "*** Video codec is open\n";

    std::cout << "*** Looking for encoder \"" << avcodec_get_name(_enc_id) << "\"...\n";
    _enc = avcodec_find_encoder(_enc_id);
    if (!_enc) {
      std::cerr << "*** Encoder was not found\n";
      return -1;
    }
    std::cout << "*** Encoder was found\n";

    std::cout << "*** Allocating encoder context...\n";
    _enc_ctx = avcodec_alloc_context3(_enc);
    if (!_enc_ctx) {
      std::cerr << "*** Failed to allocate encoder context\n";
      return -1;
    }
    std::cout << "*** Encoder context is allocated\n";

    _enc_ctx->pix_fmt = _enc_pix_fmt;
    _enc_ctx->width = _dec_ctx->width;
    _enc_ctx->height = _dec_ctx->height;
    _enc_ctx->time_base.den = 1;
    _enc_ctx->time_base.num = 1;

    std::cout << "*** Opening encoder...\n";
    AVDictionary *opts = nullptr;
    if ((ret = avcodec_open2(_enc_ctx, _enc, nullptr)) < 0) {
      std::cerr << "*** Failed to open encoder:" << avutils::error_msg(ret) << "\n";
      return ret;
    }
    std::cout << "*** Encoder is open\n";

    std::cout << "*** Opening sws...\n";
    _sws_ctx = sws_getContext(_dec_ctx->width, _dec_ctx->height, _dec_ctx->pix_fmt,
                              _enc_ctx->width, _enc_ctx->height, _enc_ctx->pix_fmt,
                              SWS_BICUBIC, NULL, NULL, NULL);
    if (!_sws_ctx) {
      std::cerr << "*** Failed to open sws\n";
      return -1;
    }
    std::cout << "*** SWS is open\n";

    std::cout << "*** Allocating frames...\n";
    _dec_frame = av_frame_alloc();
    _enc_frame = av_frame_alloc();
    if (!_dec_frame || !_enc_frame) {
      std::cerr << "*** Failed to allocate frames\n";
      return -1;
    }
    _enc_frame->format = _enc_ctx->pix_fmt;
    _enc_frame->width = _enc_ctx->width;
    _enc_frame->height = _enc_ctx->height;
    ret = av_image_alloc(_enc_frame->data, _enc_frame->linesize, _enc_ctx->width,
                         _enc_ctx->height, _enc_ctx->pix_fmt, 32);
    if (ret < 0) {
      std::cerr << "*** Could not allocate raw picture buffer:" << avutils::error_msg(ret)
                << "\n";
      return ret;
    }
    std::cout << "*** Frames were allocated\n";

    return 0;
  }

  void init_open_parameters(AVInputFormat **input_format, AVDictionary **options) {
    if (PLATFORM_APPLE) {
      *input_format = av_find_input_format("avfoundation");
      av_dict_set(options, "framerate", "30", 0);
      av_dict_set(options, "pixel_format", av_get_pix_fmt_name(_dec_pix_fmt), 0);
      av_dict_set(options, "video_size", _dimensions.c_str(), 0);
    } else {
      std::cerr << "*** Linux webcam support is not implemented yet\n";
      exit(1);
    }
  }

  void generate(int count, streams::observer<encoded_packet> &observer) {
    if (!_fmt_ctx) {
      if (init()) {
        observer.on_error(video_error::StreamInitializationError);
        return;
      }
    }

    int packets = 0;
    while (packets < count) {
      int ret = av_read_frame(_fmt_ctx, &_dec_pkt);
      if (ret < 0) {
        std::cerr << "*** Failed to read frame:" << avutils::error_msg(ret) << "\n";
        observer.on_error(video_error::FrameGenerationError);
        return;
      }

      if ((ret = avcodec_send_packet(_dec_ctx, &_dec_pkt)) != 0) {
        std::cerr << "*** avcodec_send_packet error:" << avutils::error_msg(ret) << "\n";
        observer.on_error(video_error::FrameGenerationError);
        return;
      }

      if ((ret = avcodec_receive_frame(_dec_ctx, _dec_frame)) != 0) {
        std::cerr << "*** avcodec_receive_frame error:" << avutils::error_msg(ret)
                  << "\n";
        observer.on_error(video_error::FrameGenerationError);
        return;
      }

      sws_scale(_sws_ctx, _dec_frame->data, _dec_frame->linesize, 0, _enc_frame->height,
                _enc_frame->data, _enc_frame->linesize);

      if ((ret = avcodec_send_frame(_enc_ctx, _enc_frame)) != 0) {
        std::cerr << "*** avcodec_send_frame error:" << avutils::error_msg(ret) << "\n";
        observer.on_error(video_error::FrameGenerationError);
        return;
      }

      if ((ret = avcodec_receive_packet(_enc_ctx, &_enc_pkt)) != 0) {
        std::cerr << "*** avcodec_receive_packet error:" << avutils::error_msg(ret)
                  << "\n";
        observer.on_error(video_error::FrameGenerationError);
        return;
      }

      auto release = gsl::finally([this]() {
        av_packet_unref(&_dec_pkt);
        av_packet_unref(&_enc_pkt);
      });
      observer.on_next(encoded_packet{
          encoded_frame{std::string{_enc_pkt.data, _enc_pkt.data + _enc_pkt.size}}});
      packets++;
    }
  }

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

streams::publisher<encoded_packet> camera_source(boost::asio::io_service &io,
                                                 const std::string &dimensions) {
  return streams::generators<encoded_packet>::stateful(
             [dimensions]() { return new camera_source_impl(dimensions); },
             [](camera_source_impl *impl, int count,
                streams::observer<encoded_packet> &sink) {
               return impl->generate(count, sink);
             })
         >> streams::asio::interval<encoded_packet>(io,
                                                    std::chrono::milliseconds(1000 / 25));
}

}  // namespace video
}  // namespace rtm