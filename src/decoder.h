#pragma once

// TODO: this is a copy of decoder from video-js
#include <algorithm>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

struct Image {
  AVPixelFormat format;
  int width;
  int height;

  uint8_t *data[4];
  int linesize[4];
};

class Decoder {
 public:
  Decoder(int image_width, int image_height)
      : _image_width(image_width), _image_height(image_height) {}

  ~Decoder() {
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

    return avcodec_open2(_decoder_context, _decoder, 0);
  }

  int data_received(const uint8_t *data, int length, int chunk, int chunks) {
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

  int process_frame(const uint8_t *data, int length) {
    _packet->data = (uint8_t *)data;
    _packet->size = length;
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
        fprintf(stderr, "av_image_alloc failed\n");
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

  bool frame_ready() const { return _frame_ready; }

 private:
  int _image_width{-1};
  int _image_height{-1};

  const AVPixelFormat _image_format{AV_PIX_FMT_RGB0};

  AVPacket *_packet{nullptr};
  AVFrame *_frame{nullptr};
  AVCodec *_decoder{nullptr};
  AVCodecParameters *_params{nullptr};
  AVCodecContext *_decoder_context{nullptr};
  Image *_image{nullptr};
  SwsContext *_sws_context{nullptr};
  std::unique_ptr<uint8_t[]> _extra_data;

  bool _frame_ready{false};
  std::vector<uint8_t> _chunk_buffer;
};
