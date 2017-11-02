#include "vp9_encoder.h"

extern "C" {
#include <libavutil/imgutils.h>
}

#include "avutils.h"
#include "logging.h"
#include "video_error.h"

namespace satori {
namespace video {

struct vp9_encoder {
  vp9_encoder(uint8_t lag_in_frames) : _lag_in_frames(lag_in_frames) {}

  streams::publisher<encoded_packet> init(const owned_image_frame &f) {
    CHECK(!_encoder_context);
    LOG(INFO) << "Initializing encoder";

    avutils::init();
    _encoder_context = avutils::encoder_context(_encoder_id);
    _encoder_context->width = f.width;
    _encoder_context->height = f.height;

    // http://wiki.webmproject.org/ffmpeg/vp9-encoding-guide
    AVDictionary *codec_options = nullptr;
    // TODO: pass these parameters as an input, for example, via json file.
    av_dict_set(&codec_options, "threads", "4", 0);
    av_dict_set(&codec_options, "frame-parallel", "1", 0);
    av_dict_set(&codec_options, "tile-columns", "6", 0);
    av_dict_set(&codec_options, "auto-alt-ref", "1", 0);
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

    encoded_metadata m;
    m.codec_name = "vp9";
    m.codec_data.assign(_encoder_context->extradata,
                        _encoder_context->extradata + _encoder_context->extradata_size);
    m.image_size =
        image_size{static_cast<int16_t>(f.width),
                   static_cast<int16_t>(f.height)};  // FIXME: static_cast is not good

    return streams::publishers::of({encoded_packet{m}});
  }

  streams::publisher<encoded_packet> on_image_frame(const owned_image_frame &f) {
    if (!_encoder_context) {
      auto metadata = init(f);
      auto frames = encode_frame(f);
      return streams::publishers::concat(std::move(metadata), std::move(frames));
    }

    return encode_frame(f);
  }

  streams::publisher<encoded_packet> encode_frame(const owned_image_frame &f) {
    avutils::copy_image_to_av_frame(f, _tmp_frame);
    avutils::sws_scale(_sws_context, _tmp_frame, _frame);
    avcodec_send_frame(_encoder_context.get(), _frame.get());

    std::vector<encoded_packet> packets;
    while (true) {
      AVPacket packet;
      av_init_packet(&packet);
      int ret = avcodec_receive_packet(_encoder_context.get(), &packet);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        break;
      }

      if (ret < 0) {
        return streams::publishers::error<encoded_packet>(
            video_error::FrameGenerationError);
      }

      encoded_frame frame;
      frame.data.assign(packet.data, packet.data + packet.size);
      frame.id = f.id;
      frame.timestamp = f.timestamp;
      frame.key_frame = static_cast<bool>(packet.flags & AV_PKT_FLAG_KEY);
      packets.emplace_back(std::move(frame));

      av_packet_unref(&packet);
    }

    _counter++;
    if (_counter % 100 == 0) {
      LOG(INFO) << "Encoded " << _counter << " frames";
    }
    LOG(2) << "Encoded " << _counter << " frames";

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
  int64_t _counter{0};
};  // namespace video

streams::op<owned_image_packet, encoded_packet> encode_vp9(uint8_t lag_in_frames) {
  return [lag_in_frames](streams::publisher<owned_image_packet> &&src) {
    auto encoder = new vp9_encoder(lag_in_frames);

    return std::move(src) >> streams::flat_map([encoder](owned_image_packet &&packet) {
             if (const owned_image_frame *frame =
                     boost::get<owned_image_frame>(&packet)) {
               return encoder->on_image_frame(*frame);
             }
             return streams::publishers::empty<encoded_packet>();
           })
           >> streams::do_finally([encoder]() {
               LOG(INFO) << "Deleting VP9 encoder";
               delete encoder;
             });
  };
}

}  // namespace video
}  // namespace satori
