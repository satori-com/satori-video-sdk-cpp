#include "librtmvideo/decoder.h"

#include <algorithm>
#include <memory>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include "librtmvideo/base.h"

namespace {
struct Image {
  AVPixelFormat format;
  int width;
  int height;

  uint8_t *data[4];
  int linesize[4];
};

AVPixelFormat to_av_pixel_format(image_pixel_format pixel_format) {
  switch (pixel_format) {
    case image_pixel_format::BGR:
      return AV_PIX_FMT_BGR24;
    case image_pixel_format::RGB0:
      return AV_PIX_FMT_RGB0;
    default:
      fprintf(stderr, "Unsupported pixel format: %d\n", (int)pixel_format);
      return AV_PIX_FMT_BGR24;
  }
}
}

struct decoder {
 public:
  decoder(int image_width, int image_height, AVPixelFormat image_pixel_format)
      : _image_width(image_width),
        _image_height(image_height),
        _image_format(image_pixel_format) {}

  ~decoder() {
    if (_sws_context) sws_freeContext(_sws_context);

    if (_image) {
      av_freep(&_image->data[0]);
      delete _image;
    }
    if (_decoder_context) {
      avcodec_close(_decoder_context);
      avcodec_free_context(&_decoder_context);
    }
    if (_frame) av_frame_free(&_frame);
    if (_packet) av_packet_free(&_packet);
  }

  static void init_library() {
    av_log_set_level(AV_LOG_VERBOSE);
    avcodec_register_all();
  }

  int init() {
    _packet = av_packet_alloc();
    if (!_packet) return 1;

    _frame = av_frame_alloc();
    if (!_frame) return 2;

    return 0;
  }

  int set_metadata(const char *codec_name, const uint8_t *extra_data,
                   int extra_data_length) {
    _initialized = false;
    _decoder = avcodec_find_decoder_by_name(codec_name);
    if (!_decoder) {
      fprintf(stderr, "decoder not found: %s\n", codec_name);
      return 1;
    }

    _decoder_context = avcodec_alloc_context3(_decoder);
    if (!_decoder_context) return 2;

    _params = avcodec_parameters_alloc();
    _extra_data.reset(new uint8_t[extra_data_length]);
    memcpy(_extra_data.get(), extra_data, extra_data_length);
    _params->extradata = _extra_data.get();
    _params->extradata_size = extra_data_length;
    int err = avcodec_parameters_to_context(_decoder_context, _params);
    if (err) return err;

    err = avcodec_open2(_decoder_context, _decoder, 0);
    if (err) return err;

    _initialized = true;
    return 0;
  }

  int process_frame_message(const uint8_t *data, size_t length, int chunk,
                            int chunks) {
    if (!_initialized) return -1;
    _frame_ready = false;
    if (chunks == 1) return process_frame(data, length);

    if (chunk == 1) {
      _chunk_buffer.clear();
    }
    _chunk_buffer.insert(_chunk_buffer.end(), data, data + length);
    if (chunk != chunks) return 0;

    int err = process_frame(_chunk_buffer.data(), _chunk_buffer.size());
    _chunk_buffer.clear();
    return err;
  }

  int process_frame(const uint8_t *data, size_t length) {
    _packet->data = (uint8_t *)data;
    _packet->size = (int)length;
    int err = avcodec_send_packet(_decoder_context, _packet);
    if (err) {
      fprintf(stderr, "avcodec_send_packet error: %i\n", err);
      return err;
    }

    err = avcodec_receive_frame(_decoder_context, _frame);
    if (err) {
      fprintf(stderr, "avcodec_receive_frame error: %i\n", err);
      return err;
    }

    if (!_image) {
      if (_image_width == -1) {
        _image_width = _frame->width;
        _image_height = _frame->height;
      }

      // todo: move this code into Image class.
      _image = new Image;
      _image->format = _image_format;
      _image->width = _image_width;
      _image->height = _image_height;
      int bytes = av_image_alloc(_image->data, _image->linesize, _image->width,
                                 _image->height, _image->format, 1);
      if (bytes <= 0) {
        fprintf(stderr, "av_image_alloc failed for %dx%d (%dx%d)\n",
                _image->width, _image->height, this->_image_width,
                this->_image_height);
        return 1;
      }

      _sws_context = sws_getContext(
          _frame->width, _frame->height, (AVPixelFormat)_frame->format,
          _image->width, _image->height, _image->format, SWS_FAST_BILINEAR,
          nullptr, nullptr, nullptr);

      if (!_sws_context) {
        fprintf(stderr, "_sws_context failed\n");
        return 2;
      }
    }

    sws_scale(_sws_context, _frame->data, _frame->linesize, 0, _frame->height,
              _image->data, _image->linesize);

    _frame_ready = true;
    return 0;
  }

  int image_width() const { return _image_width; }

  int image_height() const { return _image_height; }

  int stream_width() const {
    return _decoder_context ? _decoder_context->width : 0;
  }

  int stream_height() const {
    return _decoder_context ? _decoder_context->height : 0;
  }

  uint8_t *image_data() const { return _image ? _image->data[0] : nullptr; }

  uint64_t image_size() const { return _image->linesize[0] * _image->height; }

  uint64_t image_line_size() const { return _image->linesize[0]; }

  bool frame_ready() const { return _frame_ready; }

 private:
  int _image_width{-1};
  int _image_height{-1};
  const AVPixelFormat _image_format;

  AVPacket *_packet{nullptr};
  AVFrame *_frame{nullptr};
  AVCodec *_decoder{nullptr};
  AVCodecParameters *_params{nullptr};
  AVCodecContext *_decoder_context{nullptr};
  Image *_image{nullptr};
  SwsContext *_sws_context{nullptr};
  std::unique_ptr<uint8_t[]> _extra_data;
  bool _initialized{false};

  bool _frame_ready{false};
  std::vector<uint8_t> _chunk_buffer;
};

EXPORT void decoder_init_library() { decoder::init_library(); }

EXPORT decoder *decoder_new(int width, int height,
                            image_pixel_format pixel_format) {
  std::unique_ptr<decoder> d(
      new decoder(width, height, to_av_pixel_format(pixel_format)));
  int err = d->init();
  if (err) {
    fprintf(stderr, "Error initializing decoder: %d\n", err);
    return nullptr;
  }
  return d.release();
}

EXPORT void decoder_delete(decoder *d) { delete d; }

EXPORT int decoder_set_metadata(decoder *d, const char *codec_name,
                                const uint8_t *metadata, size_t len) {
  return d->set_metadata(codec_name, metadata, len);
}

EXPORT int decoder_process_frame_message(decoder *d, int64_t i1, int64_t i2,
                                         uint32_t rtp_timestamp,
                                         double ntp_timestamp,
                                         const uint8_t *frame_data, size_t len,
                                         int chunk, int chunks) {
  // todo pass the rest of parameters too.
  return d->process_frame_message(frame_data, len, chunk, chunks);
}
EXPORT bool decoder_frame_ready(decoder *d) { return d->frame_ready(); }

EXPORT int decoder_image_height(decoder *d) { return d->image_height(); }
EXPORT int decoder_image_width(decoder *d) { return d->image_width(); }
EXPORT int decoder_image_line_size(decoder *d) { return d->image_line_size(); }
EXPORT const uint8_t *decoder_image_data(decoder *d) { return d->image_data(); }
