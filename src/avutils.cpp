#include <sstream>
#include <stdexcept>

extern "C" {
#include <libavutil/pixdesc.h>
}

#include "avutils.h"
#include "logging.h"

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
      throw std::runtime_error{"Unsupported pixel format: "
                               + std::to_string((int)pixel_format)};
  }
}

std::shared_ptr<AVCodecContext> encoder_context(const AVCodecID encoder_id) {
  const std::string encoder_name = avcodec_get_name(encoder_id);
  LOG_S(INFO) << "Searching for encoder '" << encoder_name << "'";

  const AVCodec *encoder = avcodec_find_encoder(encoder_id);
  if (encoder == nullptr) {
    LOG_S(ERROR) << "Encoder '" << encoder_name << "' was not found";
    return nullptr;
  }
  LOG_S(INFO) << "Encoder '" << encoder_name << "' was found";

  if (encoder->pix_fmts == nullptr) {
    LOG_S(ERROR) << "Encoder '" << encoder_name << "' doesn't support any pixel format";
    return nullptr;
  }

  LOG_S(INFO) << "Allocating context for encoder '" << encoder_name << "'";
  AVCodecContext *encoder_context = avcodec_alloc_context3(encoder);
  if (encoder_context == nullptr) {
    LOG_S(ERROR) << "Failed to allocate context for encoder '" << encoder_name << "'";
    return nullptr;
  }
  LOG_S(INFO) << "Allocated context for encoder '" << encoder_name << "'";

  encoder_context->codec_type = AVMEDIA_TYPE_VIDEO;
  encoder_context->codec_id = encoder_id;
  encoder_context->pix_fmt = encoder->pix_fmts[0];
  encoder_context->gop_size = 12;  // I-frame at most every gop_size frames
  encoder_context->time_base = {.num = 1, .den = 1000};  // 1 millisecond
  encoder_context->bit_rate = 10000000;

  return std::shared_ptr<AVCodecContext>(encoder_context, [](AVCodecContext *ctx) {
    LOG_S(INFO) << "Deleting context for encoder '" << ctx->codec->name << "'";
    avcodec_free_context(&ctx);
  });
}

std::shared_ptr<AVFrame> av_frame(int width, int height, int align,
                                  AVPixelFormat pixel_format) {
  std::ostringstream frame_description_stream;
  frame_description_stream << width << "x" << height << ":" << align << ":"
                           << av_get_pix_fmt_name(pixel_format);

  const std::string frame_description = frame_description_stream.str();

  LOG_S(INFO) << "Allocating frame " << frame_description;
  AVFrame *frame = av_frame_alloc();
  if (frame == nullptr) {
    LOG_S(ERROR) << "Failed to allocate frame " << frame_description;
    return nullptr;
  }
  LOG_S(INFO) << "Allocated frame " << frame_description;
  std::shared_ptr<AVFrame> frame_smart_ptr =
      std::shared_ptr<AVFrame>(frame, [frame_description](AVFrame *f) {
        LOG_S(INFO) << "Deleting frame " << frame_description;
        av_frame_free(&f);
      });

  frame_smart_ptr->width = width;
  frame_smart_ptr->height = height;
  frame_smart_ptr->format = pixel_format;

  LOG_S(INFO) << "Allocating data for frame " << frame_description;
  int ret = av_frame_get_buffer(frame_smart_ptr.get(), align);
  if (ret < 0) {
    LOG_S(ERROR) << "Failed to allocate data for frame " << frame_description << ": "
                 << error_msg(ret);
    return nullptr;
  }
  LOG_S(INFO) << "Allocated data for frame " << frame_description;

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

  LOG_S(INFO) << "Allocating sws context " << context_description;
  SwsContext *sws_context = sws_getContext(
      src_frame->width, src_frame->height, (AVPixelFormat)src_frame->format,
      dst_frame->width, dst_frame->height, (AVPixelFormat)dst_frame->format, SWS_BICUBIC,
      nullptr, nullptr, nullptr);
  if (sws_context == nullptr) {
    LOG_S(ERROR) << "Failed to allocate sws context " << context_description;
    return nullptr;
  }
  LOG_S(INFO) << "Allocated sws context " << context_description;

  return std::shared_ptr<SwsContext>(sws_context, [context_description](SwsContext *ctx) {
    LOG_S(INFO) << "Deleting sws context " << context_description;
    sws_freeContext(ctx);
  });
}

void sws_scale(std::shared_ptr<SwsContext> sws_context,
               std::shared_ptr<const AVFrame> src_frame,
               std::shared_ptr<AVFrame> dst_frame) {
  sws_scale(sws_context.get(), src_frame->data, src_frame->linesize, 0, src_frame->height,
            dst_frame->data, dst_frame->linesize);
}

std::shared_ptr<AVFormatContext> format_context(
    const std::string &format, const std::string &filename,
    std::function<void(AVFormatContext *)> file_cleaner) {
  AVFormatContext *format_context;

  LOG_S(INFO) << "Allocating format context for " << filename;
  avformat_alloc_output_context2(&format_context, nullptr, format.c_str(),
                                 filename.c_str());
  if (format_context == nullptr) {
    LOG_S(ERROR) << "Failed to allocate format context for " << filename;
    return nullptr;
  }
  LOG_S(INFO) << "Allocated format context for " << filename;

  return std::shared_ptr<AVFormatContext>(
      format_context, [filename, file_cleaner](AVFormatContext *ctx) {
        LOG_S(INFO) << "Deleting format context for file " << filename;
        file_cleaner(ctx);
        avformat_free_context(ctx);  // releases streams too
      });
}

void copy_image_to_av_frame(const owned_image_frame &image,
                            std::shared_ptr<AVFrame> frame) {
  BOOST_ASSERT_MSG(image.width == frame->width, "Image and frame widhts don't match");
  BOOST_ASSERT_MSG(image.height == frame->height, "Image and frame heights don't match");
  for (int i = 0; i < MAX_IMAGE_PLANES; i++) {
    if (image.plane_strides[i] > 0) {
      memcpy(frame->data[i], image.plane_data[i].data(), image.plane_data[i].size());
    }
  }
}

}  // namespace avutils
}  // namespace video
}  // namespace rtm