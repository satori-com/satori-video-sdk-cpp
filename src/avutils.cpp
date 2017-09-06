#include <iostream>
#include <sstream>
#include <stdexcept>

extern "C" {
#include <libavutil/pixdesc.h>
}

#include "avutils.h"

namespace rtm {
namespace video {
namespace avutils {

void init() {
  static bool initialized = false;
  if (!initialized) {
    avcodec_register_all();
    av_register_all();
    initialized = true;
  }
}

std::string error_msg(const int av_error_code) {
  char av_error[AV_ERROR_MAX_STRING_SIZE];
  av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, av_error_code);
  return std::string{av_error};
}

AVPixelFormat to_av_pixel_format(const image_pixel_format pixel_format) {
  switch (pixel_format) {
    case image_pixel_format::BGR:
      return AV_PIX_FMT_BGR24;
    case image_pixel_format::RGB0:
      return AV_PIX_FMT_RGB0;
    default:
      throw std::runtime_error{"Unsupported pixel format: " +
                               std::to_string((int)pixel_format)};
  }
}

std::shared_ptr<AVCodecContext> encoder_context(const AVCodecID encoder_id) {
  const std::string encoder_name = avcodec_get_name(encoder_id);
  std::cout << "searching for encoder '" << encoder_name << "'\n";

  const AVCodec *encoder = avcodec_find_encoder(encoder_id);
  if (encoder == nullptr) {
    std::cerr << "encoder '" << encoder_name << "' was not found\n";
    return nullptr;
  }
  std::cout << "encoder '" << encoder_name << "' was found\n";

  if (encoder->pix_fmts == nullptr) {
    std::cerr << "encoder '" << encoder_name << "' doesn't support any pixel format\n";
    return nullptr;
  }

  std::cout << "allocating context for encoder '" << encoder_name << "'\n";
  AVCodecContext *encoder_context = avcodec_alloc_context3(encoder);
  if (encoder_context == nullptr) {
    std::cerr << "failed to allocate context for encoder '" << encoder_name << "'\n";
    return nullptr;
  }
  std::cout << "allocated context for encoder '" << encoder_name << "'\n";

  encoder_context->codec_type = AVMEDIA_TYPE_VIDEO;
  encoder_context->codec_id = encoder_id;
  encoder_context->pix_fmt = encoder->pix_fmts[0];
  encoder_context->gop_size = 4;  // I-frame at most every gop_size frames
  encoder_context->time_base = {.num = 1, .den = 1000};  // 1 millisecond

  return std::shared_ptr<AVCodecContext>(encoder_context, [](AVCodecContext *ctx) {
    std::cout << "Deleting context for encoder '" << ctx->codec->name << "'\n";
    avcodec_free_context(&ctx);
  });
}

std::shared_ptr<AVFrame> av_frame(int width, int height, int align,
                                  AVPixelFormat pixel_format) {
  std::ostringstream frame_description_stream;
  frame_description_stream << width << "x" << height << ":" << align << ":"
                           << av_get_pix_fmt_name(pixel_format);

  const std::string frame_description = frame_description_stream.str();

  std::cout << "allocating frame " << frame_description << "\n";
  AVFrame *frame = av_frame_alloc();
  if (frame == nullptr) {
    std::cerr << "failed to allocate frame " << frame_description << "\n";
    return nullptr;
  }
  std::cout << "allocated frame " << frame_description << "\n";
  std::shared_ptr<AVFrame> frame_smart_ptr =
      std::shared_ptr<AVFrame>(frame, [frame_description](AVFrame *f) {
        std::cout << "Deleting frame " << frame_description << "\n";
        av_frame_free(&f);
      });

  frame_smart_ptr->width = width;
  frame_smart_ptr->height = height;
  frame_smart_ptr->format = pixel_format;

  std::cout << "allocating data for frame " << frame_description << "\n";
  int ret = av_frame_get_buffer(frame_smart_ptr.get(), align);
  if (ret < 0) {
    std::cerr << "failed to allocate data for frame " << frame_description << ": "
              << error_msg(ret) << "\n";
    return nullptr;
  }
  std::cout << "allocated data for frame " << frame_description << "\n";

  return frame_smart_ptr;
}

std::shared_ptr<SwsContext> sws_context(std::shared_ptr<const AVFrame> src_frame,
                                        std::shared_ptr<const AVFrame> dst_frame) {
  std::ostringstream context_description_stream;
  context_description_stream << src_frame->width << "x" << src_frame->height << ":"
                             << av_get_pix_fmt_name((AVPixelFormat)src_frame->format)
                             << "->" << dst_frame->width << "x" << dst_frame->height
                             << ":"
                             << av_get_pix_fmt_name((AVPixelFormat)dst_frame->format);

  const std::string context_description = context_description_stream.str();

  std::cout << "allocating sws context " << context_description << "\n";
  SwsContext *sws_context = sws_getContext(
      src_frame->width, src_frame->height, (AVPixelFormat)src_frame->format,
      dst_frame->width, dst_frame->height, (AVPixelFormat)dst_frame->format, SWS_BICUBIC,
      nullptr, nullptr, nullptr);
  if (sws_context == nullptr) {
    std::cerr << "failed to allocate sws context " << context_description << "\n";
    return nullptr;
  }
  std::cout << "allocated sws context " << context_description << "\n";

  return std::shared_ptr<SwsContext>(sws_context, [context_description](SwsContext *ctx) {
    std::cout << "Deleting sws context " << context_description << "\n";
    sws_freeContext(ctx);
  });
}

void sws_scale(std::shared_ptr<SwsContext> sws_context,
               std::shared_ptr<const AVFrame> src_frame,
               std::shared_ptr<AVFrame> dst_frame) {
  sws_scale(sws_context.get(), src_frame->data, src_frame->linesize, 0, src_frame->height,
            dst_frame->data, dst_frame->linesize);
}

}  // namespace avutils
}  // namespace video
}  // namespace rtm