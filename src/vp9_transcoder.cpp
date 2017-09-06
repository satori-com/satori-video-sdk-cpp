#include <iostream>
#include <stdexcept>

extern "C" {
#include <libavutil/imgutils.h>
}

#include "avutils.h"
#include "vp9_transcoder.h"

namespace rtm {
namespace video {

vp9_transcoder::vp9_transcoder(uint8_t lag_in_frames) : _lag_in_frames(lag_in_frames) {
  avutils::init();
  _encoder_context = avutils::encoder_context(_encoder_id);
}

vp9_transcoder::~vp9_transcoder() {}

int vp9_transcoder::init() { return 0; }

void vp9_transcoder::start() {}

void vp9_transcoder::on_metadata(image_metadata &&) {}

void vp9_transcoder::on_frame(image_frame &&f) {
  const AVPixelFormat av_pixel_format = avutils::to_av_pixel_format(f.pixel_format);

  if (!_initialized) {
    std::cout << "initializing transcoder\n";
    _encoder_context->width = f.width;
    _encoder_context->height = f.height;
    AVDictionary *codec_options = nullptr;
    av_dict_set(&codec_options, "lag-in-frames", std::to_string(_lag_in_frames).c_str(),
                0);
    int ret = avcodec_open2(_encoder_context.get(), nullptr, &codec_options);
    av_dict_free(&codec_options);
    if (ret < 0) {
      throw std::runtime_error{"failed to open encoder: " + avutils::error_msg(ret)};
    }

    // TODO: make align parameterizable
    _tmp_frame = avutils::av_frame(f.width, f.height, 1, av_pixel_format);
    _frame = avutils::av_frame(f.width, f.height, 1, _encoder_context->pix_fmt);

    _sws_context = avutils::sws_context(_tmp_frame, _frame);
    if (_sws_context == nullptr) {
      throw std::runtime_error{"failed to open sws"};
    }

    source::foreach_sink([this](auto s) {
      s->on_metadata({.codec_name = "vp9",
                      .codec_data = std::string{_encoder_context->extradata,
                                                _encoder_context->extradata +
                                                    _encoder_context->extradata_size}});
    });

    _initialized = true;
  }

  for (uint8_t i = 0; i < MAX_IMAGE_PLANES; i++) {
    uint16_t stride = f.plane_strides[i];
    _tmp_frame->linesize[i] = stride;
    if (stride > 0) {
      memcpy(_tmp_frame->data[i], f.plane_data[i].data(), f.plane_data[i].size());
    }
  }

  avutils::sws_scale(_sws_context, _tmp_frame, _frame);

  avcodec_send_frame(_encoder_context.get(), _frame.get());

  while (true) {
    AVPacket packet;
    av_init_packet(&packet);
    int ret = avcodec_receive_packet(_encoder_context.get(), &packet);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      return;
    } else if (ret < 0) {
      throw std::runtime_error{"failed to get packet: " + avutils::error_msg(ret)};
    }

    const std::string data{packet.data, packet.data + packet.size};
    av_packet_unref(&packet);

    source::foreach_sink(
        [&f, &data](auto s) { s->on_frame({.data = data, .id = f.id}); });
  }
}

bool vp9_transcoder::empty() { return true; }

}  // namespace video
}  // namespace rtm