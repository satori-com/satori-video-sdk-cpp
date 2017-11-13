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
#include "satorivideo/base.h"
#include "streams/asio_streams.h"
#include "video_error.h"
#include "video_streams.h"

namespace {

uint8_t system_framerate() {
  if (PLATFORM_APPLE) {
    return 30;  // the only supported frame rate by AVFoundation
  }
  ABORT() << "Webcam support is not implemented yet";
}

}  // namespace

namespace satori {
namespace video {

struct camera_source_impl {
  explicit camera_source_impl(const std::string &resolution)
      : _resolution(resolution),
        _framerate(std::to_string(system_framerate())),
        _start(std::chrono::system_clock::now()) {}

  ~camera_source_impl() = default;

  AVInputFormat *input_format() {
    if (PLATFORM_APPLE) {
      AVInputFormat *result = av_find_input_format("avfoundation");
      CHECK(result);
      return result;
    }
    ABORT() << "Webcam support is not implemented yet";
  }

  AVDictionary *input_open_parameters() {
    if (PLATFORM_APPLE) {
      AVDictionary *options{nullptr};
      av_dict_set(&options, "framerate", _framerate.c_str(), 0);
      av_dict_set(&options, "pixel_format", av_get_pix_fmt_name(_decoder_pixel_format),
                  0);
      av_dict_set(&options, "video_size", _resolution.c_str(), 0);
      return options;
    }
    ABORT() << "Linux webcam support is not implemented yet";
  }

  int init() {
    int ret = 0;

    LOG(1) << "Looking for decoder " << avcodec_get_name(_decoder_id);
    _decoder = avcodec_find_decoder(_decoder_id);
    if (_decoder == nullptr) {
      LOG(ERROR) << "Decoder was not found";
      return -1;
    }
    LOG(1) << "Decoder was found";

    LOG(1) << "Allocating format context";
    _format_context =
        avutils::open_input_format_context("0", input_format(), input_open_parameters());
    LOG(1) << "Allocated format context";

    LOG(1) << "Setting codec to format context...";
    av_format_set_video_codec(_format_context.get(), _decoder);
    LOG(1) << "Codec was set to format context";

    LOG(1) << "Looking for best stream";
    _stream_idx = avutils::find_best_video_stream(_format_context.get(), &_decoder);
    if (_stream_idx < 0) {
      return ret;
    }
    _stream = _format_context->streams[_stream_idx];
    LOG(1) << "Best stream was found";

    LOG(1) << "Allocating decoder context...";
    _decoder_context = avutils::decoder_context(_decoder);
    if (!_decoder_context) {
      LOG(ERROR) << "Failed to allocate decoder context";
      return -1;
    }
    LOG(1) << "Decodec context is allocated";

    LOG(1) << "Copying codec parameters to decoder context...";
    if ((ret = avcodec_parameters_to_context(_decoder_context.get(), _stream->codecpar))
        < 0) {
      LOG(ERROR) << "Failed to copy codec parameters to decoder context: "
                 << avutils::error_msg(ret);
      return ret;
    }
    LOG(1) << "Codec parameters were copied to decoder context";

    LOG(1) << "Opening video decoder...";
    if ((ret = avcodec_open2(_decoder_context.get(), _decoder, nullptr)) < 0) {
      LOG(ERROR) << "Failed to open video codec: " << avutils::error_msg(ret);
      return ret;
    }
    LOG(1) << "Video decoder is open";

    LOG(1) << "Allocating frames...";
    _decoded_av_frame = avutils::av_frame(
        _decoder_context->width, _decoder_context->height, 1, _decoder_context->pix_fmt);
    _converted_av_frame = avutils::av_frame(
        _decoder_context->width, _decoder_context->height, 1, AV_PIX_FMT_BGR24);
    if (!_decoded_av_frame || !_converted_av_frame) {
      LOG(ERROR) << "Failed to allocate frames";
      return -1;
    }
    LOG(1) << "Frames were allocated";

    LOG(1) << "Allocating sws context";
    _sws_context = avutils::sws_context(_decoded_av_frame, _converted_av_frame);
    if (!_sws_context) {
      LOG(ERROR) << "Failed to allocate sws context";
      return -1;
    }
    LOG(1) << "Allocated sws context";

    return 0;
  }

  void generate_one(streams::observer<owned_image_packet> &observer) {
    if (!_format_context) {
      if (init() < 0) {
        observer.on_error(video_error::StreamInitializationError);
        return;
      }
    }

    if (!_metadata_sent) {
      send_metadata(observer);
      return;
    }

    av_init_packet(&_av_packet);
    auto release = gsl::finally([this]() { av_packet_unref(&_av_packet); });

    int ret = av_read_frame(_format_context.get(), &_av_packet);
    if (ret < 0) {
      LOG(ERROR) << "Failed to read frame: " << avutils::error_msg(ret);
      observer.on_error(video_error::FrameGenerationError);
      return;
    }

    if ((ret = avcodec_send_packet(_decoder_context.get(), &_av_packet)) != 0) {
      LOG(ERROR) << "avcodec_send_packet error: " << avutils::error_msg(ret);
      observer.on_error(video_error::FrameGenerationError);
      return;
    }

    if ((ret = avcodec_receive_frame(_decoder_context.get(), _decoded_av_frame.get()))
        != 0) {
      LOG(ERROR) << "avcodec_receive_frame error: " << avutils::error_msg(ret);
      observer.on_error(video_error::FrameGenerationError);
      return;
    }

    avutils::sws_scale(_sws_context, _decoded_av_frame, _converted_av_frame);

    owned_image_frame frame = avutils::to_image_frame(_converted_av_frame);
    frame.id = {_last_pos, _av_packet.pos};
    auto ts = 1000 * _av_packet.pts * _stream->time_base.num / _stream->time_base.den;
    frame.timestamp =
        std::chrono::system_clock::time_point{_start + std::chrono::milliseconds(ts)};

    observer.on_next(std::move(frame));
    _last_pos = _av_packet.pos + 1 /* because our intervals are [i1, i2] */;
  }

  void send_metadata(streams::observer<owned_image_packet> &observer) {
    observer.on_next(owned_image_metadata{});
    _metadata_sent = true;
  }

  const std::string _resolution;
  const std::string _framerate;

  std::shared_ptr<AVFormatContext> _format_context{nullptr};
  int _stream_idx{-1};
  AVStream *_stream{nullptr};
  const AVPixelFormat _decoder_pixel_format{
      AV_PIX_FMT_BGR0};  // rawvideo: uyvy422 yuyv422 nv12 0rgb bgr0
  const AVCodecID _decoder_id{AV_CODEC_ID_RAWVIDEO};
  AVCodec *_decoder{nullptr};  // TODO: deallocate?
  std::shared_ptr<AVCodecContext> _decoder_context{nullptr};
  AVPacket _av_packet{nullptr};
  std::shared_ptr<AVFrame> _decoded_av_frame{nullptr};
  std::shared_ptr<AVFrame> _converted_av_frame{nullptr};  // for pixel format conversion
  std::shared_ptr<SwsContext> _sws_context{nullptr};

  const std::chrono::system_clock::time_point _start;
  int64_t _last_pos{0};
  bool _metadata_sent{false};
};

streams::publisher<owned_image_packet> camera_source(boost::asio::io_service &io,
                                                     const std::string &resolution,
                                                     uint8_t fps) {
  avutils::init();

  CHECK_LE(fps, system_framerate());
  return streams::generators<owned_image_packet>::stateful(
             [resolution]() { return new camera_source_impl(resolution); },
             [](camera_source_impl *impl, streams::observer<owned_image_packet> &sink) {
               impl->generate_one(sink);
             })
         >> streams::asio::interval<owned_image_packet>(
                io, std::chrono::milliseconds(1000 / fps));
}

}  // namespace video
}  // namespace satori
