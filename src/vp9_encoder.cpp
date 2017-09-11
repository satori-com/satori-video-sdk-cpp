#include "vp9_encoder.h"

extern "C" {
#include <libavutil/imgutils.h>
}

#include "avutils.h"
#include "error.h"

namespace rtm {
namespace video {

struct vp9_encoder {
  vp9_encoder(uint8_t lag_in_frames) : _lag_in_frames(lag_in_frames) {}

  streams::publisher<encoded_packet> init(const internal_image_frame &f) {
    BOOST_ASSERT(!_encoder_context);
    std::cout << "initializing encoder\n";

    avutils::init();
    _encoder_context = avutils::encoder_context(_encoder_id);
    _encoder_context->width = f.width;
    _encoder_context->height = f.height;
    AVDictionary *codec_options = nullptr;
    av_dict_set(&codec_options, "lag-in-frames", std::to_string(_lag_in_frames).c_str(),
                0);
    int ret = avcodec_open2(_encoder_context.get(), nullptr, &codec_options);
    av_dict_free(&codec_options);
    if (ret < 0) {
      return streams::publishers::error<encoded_packet>(
          video_error::StreamInitializationError);
    }

    // TODO: make align parameterizable
    const AVPixelFormat av_pixel_format = avutils::to_av_pixel_format(f.pixel_format);
    _tmp_frame = avutils::av_frame(f.width, f.height, 1, av_pixel_format);
    _frame = avutils::av_frame(f.width, f.height, 1, _encoder_context->pix_fmt);

    _sws_context = avutils::sws_context(_tmp_frame, _frame);
    if (_sws_context == nullptr) {
      return streams::publishers::error<encoded_packet>(
          video_error::StreamInitializationError);
    }

    return streams::publishers::of({encoded_packet{encoded_metadata{
        .codec_name = "vp9",
        .codec_data = std::string{
            _encoder_context->extradata,
            _encoder_context->extradata + _encoder_context->extradata_size}}}});
  }

  streams::publisher<encoded_packet> on_image_frame(const internal_image_frame &f) {
    if (!_encoder_context) {
      auto metadata = init(f);
      auto frames = encode_frame(f);
      return streams::publishers::merge(std::move(metadata), std::move(frames));
    }

    return encode_frame(f);
  }

  streams::publisher<encoded_packet> encode_frame(const internal_image_frame &f) {
    avutils::sws_scale(_sws_context, _tmp_frame, _frame);
    avcodec_send_frame(_encoder_context.get(), _frame.get());

    std::vector<encoded_packet> packets;
    while (true) {
      AVPacket packet;
      av_init_packet(&packet);
      int ret = avcodec_receive_packet(_encoder_context.get(), &packet);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        break;
      } else if (ret < 0) {
        return streams::publishers::error<encoded_packet>(
            video_error::FrameGenerationError);
      }

      const std::string data{packet.data, packet.data + packet.size};
      av_packet_unref(&packet);
      packets.push_back(encoded_frame{.data = data, .id = f.id});
    }

    return streams::publishers::of(std::move(packets));
  }

  // https://www.webmproject.org/docs/encoder-parameters/
  // The --lag-in-frames parameter defines an upper limit on the number of frames into the
  // future that the encoder can look.
  const uint8_t _lag_in_frames;
  const AVCodecID _encoder_id{AV_CODEC_ID_VP9};
  std::shared_ptr<AVCodecContext> _encoder_context{nullptr};
  std::shared_ptr<AVFrame> _tmp_frame{nullptr};  // for pixel format conversion
  std::shared_ptr<AVFrame> _frame{nullptr};
  std::shared_ptr<SwsContext> _sws_context{nullptr};
};

streams::op<image_packet, encoded_packet> encode_vp9(uint8_t lag_in_frames) {
  return [lag_in_frames](streams::publisher<image_packet> &&src) {
    vp9_encoder *encoder = new vp9_encoder(lag_in_frames);

    return std::move(src) >> streams::flat_map([encoder](image_packet &&packet) {
             if (const internal_image_frame *frame =
                     boost::get<internal_image_frame>(&packet)) {
               return encoder->on_image_frame(*frame);
             }
             return streams::publishers::empty<encoded_packet>();
           }) >>
           streams::do_finally([encoder]() { delete encoder; });
  };
};

}  // namespace video
}  // namespace rtm
