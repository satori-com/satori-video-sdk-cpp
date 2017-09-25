#include "avutils.h"
#include "error.h"
#include "librtmvideo/tele.h"
#include "stopwatch.h"
#include "video_streams.h"

namespace rtm {
namespace video {

namespace {

constexpr double epsilon = .000001;

auto frames_received = tele::counter_new("decoder", "frames_received");
auto messages_received = tele::counter_new("decoder", "messages_received");
auto messages_dropped = tele::counter_new("decoder", "messages_dropped");
auto bytes_received = tele::counter_new("decoder", "bytes_received");
auto decoding_times_millis = tele::distribution_new("decoder", "decoding_times_millis");

struct image_decoder_impl {
  image_decoder_impl(image_pixel_format pixel_format, int bounding_width,
                     int bounding_height, bool keep_proportions)
      : _pixel_format(pixel_format),
        _bounding_width(bounding_width),
        _bounding_height(bounding_height),
        _keep_proportions(keep_proportions) {}

  streams::publisher<owned_image_packet> on_metadata(const encoded_metadata &m) {
    LOG_S(1) << "received stream metadata";
    if (m.codec_data == _metadata.codec_data && m.codec_name == _metadata.codec_name) {
      return streams::publishers::empty<owned_image_packet>();
    }

    _metadata = m;
    _context = avutils::decoder_context(m.codec_name, m.codec_data);
    _packet = avutils::av_packet();
    _frame = avutils::av_frame();
    if (!_context || !_packet || !_frame) {
      return streams::publishers::error<owned_image_packet>(
          video_error::StreamInitializationError);
    }

    LOG_S(INFO) << _metadata.codec_name << " video decoder initialized";
    return streams::publishers::empty<owned_image_packet>();
  }

  // todo: drain ffmpeg on stream end.
  streams::publisher<owned_image_packet> on_image_frame(const encoded_frame &f) {
    tele::counter_inc(messages_received);
    tele::counter_inc(bytes_received, f.data.size());

    if (!_context) {
      tele::counter_inc(messages_dropped);
      return streams::publishers::empty<owned_image_packet>();
    }

    {
      stopwatch<> s;
      _packet->data = (uint8_t *)f.data.data();
      _packet->size = static_cast<int>(f.data.size());
      int err = avcodec_send_packet(_context.get(), _packet.get());
      if (err) {
        LOG_S(ERROR) << "avcodec_send_packet error: " << avutils::error_msg(err);
        return streams::publishers::error<owned_image_packet>(
            video_error::FrameGenerationError);
      }

      err = avcodec_receive_frame(_context.get(), _frame.get());
      if (err) {
        switch (err) {
          case AVERROR(EAGAIN):
            LOG_S(4) << "eagain";
            return streams::publishers::empty<owned_image_packet>();
          default:
            LOG_S(ERROR) << "avcodec_receive_frame error: " << avutils::error_msg(err);
        }
      }

      if (!_image) {
        if (!init_image()) {
          return streams::publishers::error<owned_image_packet>(
              video_error::FrameGenerationError);
        }
      }

      sws_scale(_sws_context.get(), _frame->data, _frame->linesize, 0, _frame->height,
                _image->data, _image->linesize);

      owned_image_frame frame{.id = f.id,
                              .pixel_format = _pixel_format,
                              .width = static_cast<uint16_t>(_image_width),
                              .height = static_cast<uint16_t>(_image_height),
                              .timestamp = f.timestamp};

      for (uint8_t i = 0; i < MAX_IMAGE_PLANES; i++) {
        const uint32_t plane_stride = static_cast<uint32_t>(_image->linesize[i]);
        const uint8_t *plane_data = _image->data[i];
        frame.plane_strides[i] = plane_stride;
        if (plane_stride > 0) {
          frame.plane_data[i].assign(plane_data,
                                     plane_data + (plane_stride * _image_height));
        }
      }
      tele::distribution_add(decoding_times_millis, s.millis());
      tele::counter_inc(frames_received);

      return streams::publishers::of({owned_image_packet{frame}});
    }
  }

  bool init_image() {
    _image_width = _bounding_width != -1 ? _bounding_width : _frame->width;
    _image_height = _bounding_height != -1 ? _bounding_height : _frame->height;

    if (_keep_proportions) {
      double frame_ratio = (double)_frame->width / (double)_frame->height;
      double requested_ratio = (double)_image_width / (double)_image_height;

      if (std::fabs(frame_ratio - requested_ratio) > epsilon) {
        if (frame_ratio > requested_ratio) {
          _image_height = (int)((double)_image_width / frame_ratio);
        } else {
          _image_width = (int)((double)_image_height * frame_ratio);
        }
      }
    }

    LOG_S(INFO) << "decoder resolution is " << _image_width << "x" << _image_height;

    _image = avutils::allocate_image(_image_width, _image_height, _pixel_format);
    if (!_image) {
      LOG_S(ERROR) << "allocate_image failed";
      return false;
    }

    _sws_context = avutils::sws_context(
        _frame->width, _frame->height, (AVPixelFormat)_frame->format, _image_width,
        _image_height, avutils::to_av_pixel_format(_pixel_format));

    if (!_sws_context) {
      LOG_S(ERROR) << "sws_context failed";
      return false;
    }

    return true;
  }

 private:
  const image_pixel_format _pixel_format;
  const int _bounding_width;
  const int _bounding_height;
  const bool _keep_proportions;
  std::shared_ptr<AVCodecContext> _context;
  std::shared_ptr<AVPacket> _packet;
  std::shared_ptr<AVFrame> _frame;
  std::shared_ptr<avutils::allocated_image> _image;
  int _image_width{0};
  int _image_height{0};
  std::shared_ptr<SwsContext> _sws_context;
  encoded_metadata _metadata;
};

}  // namespace

streams::op<encoded_packet, owned_image_packet> decode_image_frames(
    int bounding_width, int bounding_height, image_pixel_format pixel_format) {
  avutils::init();

  return [bounding_width, bounding_height,
          pixel_format](streams::publisher<encoded_packet> &&src) {
    image_decoder_impl *impl =
        new image_decoder_impl(pixel_format, bounding_width, bounding_height, true);
    streams::publisher<owned_image_packet> result =
        std::move(src) >> streams::flat_map([impl](encoded_packet &&packet) {
          if (const encoded_metadata *m = boost::get<encoded_metadata>(&packet)) {
            return impl->on_metadata(*m);
          } else if (const encoded_frame *f = boost::get<encoded_frame>(&packet)) {
            return impl->on_image_frame(*f);
          } else {
            BOOST_VERIFY_MSG(false, "Bad variant");
          }
        })
        >> streams::do_finally([impl]() { delete impl; });

    return result;
  };
}

}  // namespace video
}  // namespace rtm