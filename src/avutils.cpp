#include "avutils.h"

#include <chrono>
#include <sstream>
#include <stdexcept>

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
#include <libavutil/parseutils.h>
#include <libavutil/pixdesc.h>
}

#include "logging.h"
#include "satorivideo/base.h"

namespace satori {
namespace video {
namespace avutils {

namespace {

std::string to_av_codec_name(const std::string &codec_name) {
  if (codec_name == "vp9") {
    return "libvpx-vp9";
  }
  return codec_name;
}

void dump_iformats() {
  AVInputFormat *f{nullptr};
  while (true) {
    f = av_iformat_next(f);
    if (f == nullptr) {
      break;
    }
    LOG(1) << "available iformat: " << f->name;
  }
}

void dump_oformats() {
  AVOutputFormat *f{nullptr};
  while (true) {
    f = av_oformat_next(f);
    if (f == nullptr) {
      break;
    }
    LOG(1) << "available oformat: " << f->name;
  }
}

void dump_codecs() {
  AVCodec *c{nullptr};
  while (true) {
    c = av_codec_next(c);
    if (c == nullptr) {
      break;
    }
    LOG(1) << "available codec: " << c->name << " is_encoder=" << av_codec_is_encoder(c)
           << " is_decoder=" << av_codec_is_decoder(c);
  }
}

void dump_bsfs() {
  void *ptr{nullptr};
  while (true) {
    const AVBitStreamFilter *f = av_bsf_next(&ptr);
    if (f == nullptr) {
      break;
    }
    LOG(1) << "available bsf: " << f->name;
  }
}

}  // namespace

void init() {
  static bool initialized = false;
  if (!initialized) {
    loguru::Verbosity verbosity_cutoff = loguru::current_verbosity_cutoff();
    int av_log_level = AV_LOG_INFO;

    switch (verbosity_cutoff) {
      case -3:
        av_log_level = AV_LOG_FATAL;
        break;
      case -2:
        av_log_level = AV_LOG_ERROR;
        break;
      case -1:
        av_log_level = AV_LOG_WARNING;
        break;
      case 0:
        av_log_level = AV_LOG_INFO;
        break;
      case 1:
        av_log_level = AV_LOG_VERBOSE;
        break;
      case 2:
        av_log_level = AV_LOG_DEBUG;
        break;
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      case 8:
      case 9:
        av_log_level = AV_LOG_TRACE;
        break;
    }

    av_log_set_level(av_log_level);
    LOG(INFO) << "initializing av library, logging level " << av_log_level;

    avdevice_register_all();
    avcodec_register_all();
    av_register_all();
    avformat_network_init();
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

image_pixel_format to_image_pixel_format(AVPixelFormat pixel_format) {
  switch (pixel_format) {
    case AV_PIX_FMT_BGR24:
      return image_pixel_format::BGR;
    case AV_PIX_FMT_RGB0:
      return image_pixel_format::RGB0;
    default:
      throw std::runtime_error{"Unsupported pixel format: "
                               + std::to_string((int)pixel_format)};
  }
}

std::shared_ptr<AVCodecContext> encoder_context(const AVCodecID codec_id) {
  const std::string encoder_name = avcodec_get_name(codec_id);
  LOG(1) << "Searching for encoder '" << encoder_name << "'";

  const AVCodec *encoder = avcodec_find_encoder(codec_id);
  if (encoder == nullptr) {
    LOG(ERROR) << "Encoder '" << encoder_name << "' was not found";
    return nullptr;
  }
  LOG(1) << "Encoder '" << encoder_name << "' was found";

  if (encoder->pix_fmts == nullptr) {
    LOG(ERROR) << "Encoder '" << encoder_name << "' doesn't support any pixel format";
    return nullptr;
  }

  LOG(1) << "Allocating context for encoder '" << encoder_name << "'";
  AVCodecContext *encoder_context = avcodec_alloc_context3(encoder);
  if (encoder_context == nullptr) {
    LOG(ERROR) << "Failed to allocate context for encoder '" << encoder_name << "'";
    return nullptr;
  }
  LOG(1) << "Allocated context for encoder '" << encoder_name << "'";

  encoder_context->codec_type = AVMEDIA_TYPE_VIDEO;
  encoder_context->codec_id = codec_id;
  encoder_context->pix_fmt = encoder->pix_fmts[0];
  encoder_context->gop_size = 12;  // I-frame at most every gop_size frames
  // Setting timebase to 1 millisecond
  encoder_context->time_base.num = 1;
  encoder_context->time_base.den = 1000;
  encoder_context->bit_rate = 10000000;

  return std::shared_ptr<AVCodecContext>(encoder_context, [](AVCodecContext *ctx) {
    LOG(1) << "Deleting context for encoder '" << ctx->codec->name << "'";
    avcodec_free_context(&ctx);
  });
}

std::shared_ptr<AVCodecContext> decoder_context(const std::string &codec_name,
                                                gsl::cstring_span<> extra_data) {
  std::string av_codec_name = to_av_codec_name(codec_name);
  LOG(1) << "searching for decoder '" << av_codec_name << "'";
  const AVCodec *decoder = avcodec_find_decoder_by_name(av_codec_name.c_str());
  if (decoder == nullptr) {
    LOG(ERROR) << "decoder '" << av_codec_name << "' was not found";
    return nullptr;
  }

  auto context = decoder_context(decoder);

  AVCodecParameters *params = avcodec_parameters_alloc();
  if (params == nullptr) {
    LOG(ERROR) << "Failed to allocate params";
    return nullptr;
  }

  params->extradata = (uint8_t *)extra_data.data();
  params->extradata_size = static_cast<int>(extra_data.size_bytes());
  int err = avcodec_parameters_to_context(context.get(), params);
  params->extradata = nullptr;  // avcodec_parameters_free will try to free it.
  params->extradata_size = 0;
  avcodec_parameters_free(&params);

  if (err < 0) {
    LOG(ERROR) << "Failed to copy params: " << error_msg(err);
    return nullptr;
  }

  context->thread_count = 4;
  context->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

  err = avcodec_open2(context.get(), decoder, nullptr);
  if (err < 0) {
    LOG(ERROR) << "Failed to open codec: " << error_msg(err);
    return nullptr;
  }

  LOG(1) << "Allocated context for decoder '" << av_codec_name << "'";
  return context;
}

std::shared_ptr<AVCodecContext> decoder_context(const AVCodec *decoder) {
  LOG(1) << "allocating context for decoder '" << decoder->name << "'";
  std::shared_ptr<AVCodecContext> context(
      avcodec_alloc_context3(decoder), [](AVCodecContext *ctx) {
        LOG(1) << "deleting context for decoder '" << ctx->codec->name << "'";
        avcodec_free_context(&ctx);
      });
  if (context == nullptr) {
    LOG(ERROR) << "failed to allocate context for decoder '" << decoder->name << "'";
  }
  return context;
}
std::shared_ptr<AVFrame> av_frame() {
  LOG(1) << "allocating frame";
  std::shared_ptr<AVFrame> frame(av_frame_alloc(), [](AVFrame *f) {
    LOG(1) << "deleting frame";
    av_frame_free(&f);
  });
  if (frame == nullptr) {
    LOG(ERROR) << "failed to allocate frame";
    return nullptr;
  }
  LOG(1) << "allocated frame";
  return frame;
}

std::shared_ptr<AVPacket> av_packet() {
  LOG(1) << "allocating packet";
  std::shared_ptr<AVPacket> packet(av_packet_alloc(), [](AVPacket *f) {
    LOG(1) << "deleting packet";
    av_packet_free(&f);
  });
  if (packet == nullptr) {
    LOG(ERROR) << "failed to allocate packet";
    return nullptr;
  }
  LOG(1) << "allocated packet";
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

  LOG(1) << "Allocating data for frame " << frame_description;
  int ret = av_frame_get_buffer(frame_smart_ptr.get(), align);
  if (ret < 0) {
    LOG(ERROR) << "Failed to allocate data for frame " << frame_description << ": "
               << error_msg(ret);
    return nullptr;
  }
  LOG(1) << "Allocated data for frame " << frame_description;

  return frame_smart_ptr;
}

std::shared_ptr<SwsContext> sws_context(int src_width, int src_height,
                                        AVPixelFormat src_format, int dst_width,
                                        int dst_height, AVPixelFormat dst_format) {
  std::ostringstream context_description_stream;
  const char *src_fmt_name = av_get_pix_fmt_name(src_format);
  const char *dst_fmt_name = av_get_pix_fmt_name(dst_format);
  context_description_stream << src_width << "x" << src_height << ":"
                             << (src_fmt_name != nullptr ? src_fmt_name : "unknown")
                             << "->" << dst_width << "x" << dst_height << ":"
                             << (dst_fmt_name != nullptr ? dst_fmt_name : "unknown");

  const std::string context_description = context_description_stream.str();

  LOG(1) << "allocating sws context " << context_description;
  SwsContext *sws_context =
      sws_getContext(src_width, src_height, src_format, dst_width, dst_height, dst_format,
                     SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
  if (sws_context == nullptr) {
    LOG(ERROR) << "failed to allocate sws context " << context_description;
    return nullptr;
  }
  LOG(1) << "allocated sws context " << context_description;

  return std::shared_ptr<SwsContext>(sws_context, [context_description](SwsContext *ctx) {
    LOG(1) << "deleting sws context " << context_description;
    sws_freeContext(ctx);
  });
}

std::shared_ptr<SwsContext> sws_context(const std::shared_ptr<const AVFrame> &src_frame,
                                        const std::shared_ptr<const AVFrame> &dst_frame) {
  return sws_context(src_frame->width, src_frame->height,
                     (AVPixelFormat)src_frame->format, dst_frame->width,
                     dst_frame->height, (AVPixelFormat)dst_frame->format);
}

void sws_scale(const std::shared_ptr<SwsContext> &sws_context,
               const std::shared_ptr<const AVFrame> &src_frame,
               const std::shared_ptr<AVFrame> &dst_frame) {
  sws_scale(sws_context.get(), src_frame->data, src_frame->linesize, 0, src_frame->height,
            dst_frame->data, dst_frame->linesize);
}

std::shared_ptr<AVFormatContext> output_format_context(
    const std::string &format, const std::string &filename,
    const std::function<void(AVFormatContext *)> &file_cleaner) {
  AVFormatContext *format_context;

  LOG(1) << "Allocating format context for " << filename;
  avformat_alloc_output_context2(&format_context, nullptr, format.c_str(),
                                 filename.c_str());
  if (format_context == nullptr) {
    LOG(ERROR) << "Failed to allocate format context for " << filename;
    return nullptr;
  }
  LOG(1) << "Allocated format context for " << filename;

  return std::shared_ptr<AVFormatContext>(
      format_context, [filename, file_cleaner](AVFormatContext *ctx) {
        LOG(1) << "Deleting format context for file " << filename;
        file_cleaner(ctx);
        avformat_free_context(ctx);  // releases streams too
      });
}

std::shared_ptr<AVFormatContext> open_input_format_context(const std::string &url,
                                                           AVInputFormat *forced_format,
                                                           AVDictionary *options) {
  AVFormatContext *format_context = avformat_alloc_context();
  if (format_context == nullptr) {
    LOG(ERROR) << "failed to allocate format context";
    return nullptr;
  }

  std::string options_str;
  if (options != nullptr) {
    char *buffer;
    av_dict_get_string(options, &buffer, '=', ',');
    options_str = buffer;
    free(buffer);
  }

  LOG(1) << "opening url " << url << " " << options_str;
  int ret = avformat_open_input(&format_context, url.c_str(), forced_format, &options);
  if (ret < 0) {
    // format_context is freed on open error.
    LOG(ERROR) << "failed to open " << url << ": " << error_msg(ret);
    return nullptr;
  }
  LOG(1) << "opened url " << url;
  return std::shared_ptr<AVFormatContext>(format_context, [url](AVFormatContext *ctx) {
    LOG(INFO) << "closing url " << url;
    avformat_close_input(&ctx);
    avformat_free_context(ctx);
    LOG(INFO) << "format context is destroyed for " << url;
  });
}

void copy_image_to_av_frame(const owned_image_frame &image,
                            const std::shared_ptr<AVFrame> &frame) {
  CHECK_EQ(image.width, frame->width) << "Image and frame widhts don't match";
  CHECK_EQ(image.height, frame->height) << "Image and frame heights don't match";
  for (int i = 0; i < max_image_planes; i++) {
    if (image.plane_strides[i] > 0) {
      memcpy(frame->data[i], image.plane_data[i].data(), image.plane_data[i].size());
    }
  }
}

owned_image_frame to_image_frame(const std::shared_ptr<const AVFrame> &frame) {
  owned_image_frame image;

  image.width = static_cast<uint16_t>(frame->width);
  image.height = static_cast<uint16_t>(frame->height);
  image.pixel_format = to_image_pixel_format(static_cast<AVPixelFormat>(frame->format));
  image.timestamp =
      std::chrono::system_clock::time_point{std::chrono::milliseconds(frame->pts)};

  for (uint8_t i = 0; i < max_image_planes; i++) {
    const auto plane_stride = static_cast<uint32_t>(frame->linesize[i]);
    image.plane_strides[i] = plane_stride;
    if (plane_stride > 0) {
      const uint8_t *plane_data = frame->data[i];
      image.plane_data[i].assign(plane_data, plane_data + (plane_stride * frame->height));
    }
  }

  return image;
}

std::shared_ptr<allocated_image> allocate_image(const image_size &size,
                                                image_pixel_format pixel_format) {
  uint8_t *data[max_image_planes];
  int linesize[max_image_planes];

  int bytes = av_image_alloc(data, linesize, size.width, size.height,
                             to_av_pixel_format(pixel_format), 1);
  if (bytes <= 0) {
    LOG(ERROR) << "av_image_alloc failed for " << size
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

streams::error_or<image_size> parse_image_size(const std::string &str) {
  if (str == "original") {
    return image_size{original_image_width, original_image_height};
  }

  int width;
  int height;
  int ret = av_parse_video_size(&width, &height, str.c_str());
  if (ret < 0) {
    LOG(ERROR) << "couldn't parse image size from " << str << ", " << error_msg(ret);
    return std::system_category().default_error_condition(EBADMSG);
  }

  return image_size{(int16_t)width, (int16_t)height};
}

int find_best_video_stream(AVFormatContext *context, AVCodec **decoder_out) {
  int ret = avformat_find_stream_info(context, nullptr);
  if (ret < 0) {
    LOG(ERROR) << "could not find stream information:" << avutils::error_msg(ret);
    return ret;
  }

  ret = av_find_best_stream(context, AVMEDIA_TYPE_VIDEO, -1, -1, decoder_out, 0);
  if (ret < 0) {
    LOG(ERROR) << "could not find video stream:" << avutils::error_msg(ret);
    return ret;
  }
  return ret;
}

AVCodecID codec_id(const std::string &codec_name) {
  if (codec_name == "vp8") {
    return AV_CODEC_ID_VP8;
  }
  if (codec_name == "vp9") {
    return AV_CODEC_ID_VP9;
  }
  if (codec_name == "h264") {
    return AV_CODEC_ID_H264;
  }
  ABORT() << "unsupported codec: " << codec_name;
}

}  // namespace avutils
}  // namespace video
}  // namespace satori