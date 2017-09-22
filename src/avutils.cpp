#include <sstream>
#include <stdexcept>

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

#include "avutils.h"
#include "logging.h"

namespace rtm {
namespace video {
namespace avutils {

namespace {

std::string to_av_codec_name(const std::string &codec_name) {
  if (codec_name == "vp9") return "libvpx-vp9";
  return codec_name;
}

void dump_iformats() {
  AVInputFormat *f{nullptr};
  while (true) {
    f = av_iformat_next(f);
    if (!f) break;
    LOG_S(1) << "available iformat: " << f->name;
  }
}

void dump_oformats() {
  AVOutputFormat *f{nullptr};
  while (true) {
    f = av_oformat_next(f);
    if (!f) break;
    LOG_S(1) << "available oformat: " << f->name;
  }
}

void dump_codecs() {
  AVCodec *c{nullptr};
  while (true) {
    c = av_codec_next(c);
    if (!c) break;
    LOG_S(1) << "available codec: " << c->name
             << " is_encoder=" << av_codec_is_encoder(c)
             << " is_decoder=" << av_codec_is_decoder(c);
  }
}

void dump_bsfs() {
  void *ptr{nullptr};
  while (true) {
    const AVBitStreamFilter *f = av_bsf_next(&ptr);
    if (!f) break;
    LOG_S(1) << "available bsf: " << f->name;
  }
}

}  // namespace

void init() {
  static bool initialized = false;
  if (!initialized) {
    LOG_S(INFO) << "initializing av library";
    avdevice_register_all();
    avcodec_register_all();
    av_register_all();
    initialized = true;

    dump_codecs();
    dump_iformats();
    dump_oformats();
    dump_bsfs();
  }
}

std::string error_msg(const int av_error_code) {
  char av_error[AV_ERROR_MAX_STRING_SIZE];
  av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, av_error_code);
  return std::string{av_error};
}

AVPixelFormat to_av_pixel_format(image_pixel_format pixel_format) {
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

std::shared_ptr<AVCodecContext> decoder_context(const std::string &codec_name,
                                                gsl::cstring_span<> extra_data) {
  std::string av_codec_name = to_av_codec_name(codec_name);
  LOG_S(1) << "searching for decoder '" << av_codec_name << "'";
  const AVCodec *decoder = avcodec_find_decoder_by_name(av_codec_name.c_str());
  if (decoder == nullptr) {
    LOG_S(ERROR) << "decoder '" << av_codec_name << "' was not found";
    return nullptr;
  }

  LOG_S(1) << "allocating context for decoder '" << av_codec_name << "'";
  std::shared_ptr<AVCodecContext> context(
      avcodec_alloc_context3(decoder), [](AVCodecContext *ctx) {
        LOG_S(1) << "deleting context for decoder '" << ctx->codec->name << "'";
        avcodec_close(ctx);
        avcodec_free_context(&ctx);
      });
  if (context == nullptr) {
    LOG_S(ERROR) << "failed to allocate context for decoder '" << av_codec_name << "'";
    return nullptr;
  }

  AVCodecParameters *params = avcodec_parameters_alloc();
  if (params == nullptr) {
    LOG_S(ERROR) << "Failed to allocate params";
    return nullptr;
  }

  params->extradata = (uint8_t *)extra_data.data();
  params->extradata_size = static_cast<int>(extra_data.size_bytes());
  int err = avcodec_parameters_to_context(context.get(), params);
  if (err) {
    LOG_S(ERROR) << "Failed to copy params: " << error_msg(err);
    return nullptr;
  }

  context->thread_count = 4;
  context->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

  err = avcodec_open2(context.get(), decoder, 0);
  if (err) {
    LOG_S(ERROR) << "Failed to open codec: " << error_msg(err);
    return nullptr;
  }

  LOG_S(1) << "Allocated context for decoder '" << av_codec_name << "'";
  return context;
}

std::shared_ptr<AVFrame> av_frame() {
  LOG_S(1) << "allocating frame";
  std::shared_ptr<AVFrame> frame(av_frame_alloc(), [](AVFrame *f) {
    LOG_S(1) << "deleting frame";
    av_frame_free(&f);
  });
  if (frame == nullptr) {
    LOG_S(ERROR) << "failed to allocate frame";
    return nullptr;
  }
  LOG_S(1) << "allocated frame";
  return frame;
}

std::shared_ptr<AVPacket> av_packet() {
  LOG_S(1) << "allocating packet";
  std::shared_ptr<AVPacket> packet(av_packet_alloc(), [](AVPacket *f) {
    LOG_S(1) << "deleting packet";
    av_packet_free(&f);
  });
  if (packet == nullptr) {
    LOG_S(ERROR) << "failed to allocate packet";
    return nullptr;
  }
  LOG_S(1) << "allocated frame";
  return packet;
}

std::shared_ptr<AVFrame> av_frame(int width, int height, int align,
                                  AVPixelFormat pixel_format) {
  std::shared_ptr<AVFrame> frame_smart_ptr = av_frame();
  if (frame_smart_ptr == nullptr) {
    return nullptr;
  }

  std::ostringstream frame_description_stream;
  frame_description_stream << width << "x" << height << ":" << align << ":"
                           << av_get_pix_fmt_name(pixel_format);

  const std::string frame_description = frame_description_stream.str();

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

std::shared_ptr<SwsContext> sws_context(int src_width, int src_height,
                                        AVPixelFormat src_format, int dst_width,
                                        int dst_height, AVPixelFormat dst_format) {
  std::ostringstream context_description_stream;
  context_description_stream << src_width << "x" << src_height << ":"
                             << av_get_pix_fmt_name(src_format) << "->" << dst_width
                             << "x" << dst_height << ":"
                             << av_get_pix_fmt_name(dst_format);

  const std::string context_description = context_description_stream.str();

  LOG_S(1) << "allocating sws context " << context_description;
  SwsContext *sws_context =
      sws_getContext(src_width, src_height, src_format, dst_width, dst_height, dst_format,
                     SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
  if (sws_context == nullptr) {
    LOG_S(ERROR) << "failed to allocate sws context " << context_description;
    return nullptr;
  }
  LOG_S(1) << "allocated sws context " << context_description;

  return std::shared_ptr<SwsContext>(sws_context, [context_description](SwsContext *ctx) {
    LOG_S(1) << "deleting sws context " << context_description;
    sws_freeContext(ctx);
  });
}

std::shared_ptr<SwsContext> sws_context(std::shared_ptr<const AVFrame> src_frame,
                                        std::shared_ptr<const AVFrame> dst_frame) {
  return sws_context(src_frame->width, src_frame->height,
                     (AVPixelFormat)src_frame->format, dst_frame->width,
                     dst_frame->height, (AVPixelFormat)dst_frame->format);
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
std::shared_ptr<allocated_image> allocate_image(int width, int height,
                                                image_pixel_format pixel_format) {
  uint8_t *data[MAX_IMAGE_PLANES];
  int linesize[MAX_IMAGE_PLANES];

  int bytes =
      av_image_alloc(data, linesize, width, height, to_av_pixel_format(pixel_format), 1);
  if (bytes <= 0) {
    LOG_S(ERROR) << "av_image_alloc failed for " << width << "x" << height
                 << " format= " << (int)pixel_format;
    return nullptr;
  }

  auto image = new allocated_image;
  memcpy(image->data, data, sizeof(data));
  memcpy(image->linesize, linesize, sizeof(linesize));
  return std::shared_ptr<allocated_image>(image, [](allocated_image *img) {
    av_freep(&img->data[0]);
    delete img;
  });
}

}  // namespace avutils
}  // namespace video
}  // namespace rtm