#include "avutils.h"
#include "satorivideo/tele.h"
#include "stopwatch.h"
#include "video_error.h"
#include "video_streams.h"

namespace satori {
namespace video {

namespace {

constexpr double epsilon = .000001;

auto frames_received = tele::counter_new("decoder", "frames_received");
auto messages_received = tele::counter_new("decoder", "messages_received");
auto messages_dropped = tele::counter_new("decoder", "messages_dropped");
auto bytes_received = tele::counter_new("decoder", "bytes_received");
auto send_packet_millis = tele::distribution_new("decoder", "send_packet_millis");
auto receive_frame_millis = tele::distribution_new("decoder", "receive_frame_millis");

struct image_decoder_op {
  image_decoder_op(image_pixel_format pixel_format, int bounding_width,
                   int bounding_height, bool keep_proportions)
      : _pixel_format(pixel_format),
        _bounding_width(bounding_width),
        _bounding_height(bounding_height),
        _keep_proportions(keep_proportions) {}

  template <typename T>
  struct instance : streams::subscriber<encoded_packet>,
                    streams::impl::drain_source_impl<owned_image_packet> {
    static_assert(std::is_same<T, encoded_packet>::value, "types mismatch");
    using value_t = owned_image_packet;

    static streams::publisher<owned_image_packet> apply(
        streams::publisher<encoded_packet> &&source, image_decoder_op &&op) {
      return streams::publisher<owned_image_packet>(
          new streams::impl::op_publisher<T, owned_image_packet, image_decoder_op>(
              std::move(source), std::move(op)));
    }

    instance(image_decoder_op &&op, streams::subscriber<owned_image_packet> &sink)
        : streams::impl::drain_source_impl<owned_image_packet>(sink),
          _pixel_format(op._pixel_format),
          _bounding_width(op._bounding_width),
          _bounding_height(op._bounding_height),
          _keep_proportions(op._keep_proportions) {}

    ~instance() {
      if (_source) {
        _source->cancel();
      }
    }

    void on_subscribe(streams::subscription &s) override {
      LOG(4) << "on_subscribe";
      _source = &s;
      deliver_on_subscribe();
    }

    void on_next(encoded_packet &&pkt) override {
      LOG(4) << this << " on_next";
      if (const encoded_metadata *m = boost::get<encoded_metadata>(&pkt)) {
        on_metadata(*m);
      } else if (const encoded_frame *f = boost::get<encoded_frame>(&pkt)) {
        on_image_frame(*f);
      } else {
        ABORT_S() << "Bad variant";
      }

      drain();
    }

    void on_complete() override {
      LOG(4) << this << " on_complete";
      _source = nullptr;
      if (_context) {
        int err = avcodec_send_packet(_context.get(), nullptr);
        if (err) {
              LOG(ERROR) << "avcodec_send_packet final error: " << avutils::error_msg(err);
          deliver_on_error(video_error::FrameGenerationError);
          return;
        }
        drain();
      } else {
        deliver_on_complete();
      }
    }

    void on_error(std::error_condition ec) override {
      _source = nullptr;
      deliver_on_error(ec);
    }

    void on_metadata(const encoded_metadata &m) {
      LOG(1) << this << "received stream metadata";
      if (m.codec_data == _metadata.codec_data && m.codec_name == _metadata.codec_name) {
        return;
      }

      _metadata = m;
      _context = avutils::decoder_context(m.codec_name, m.codec_data);
      _packet = avutils::av_packet();
      _frame = avutils::av_frame();
      if (!_context || !_packet || !_frame) {
        deliver_on_error(video_error::StreamInitializationError);
        return;
      }

      LOG(INFO) << _metadata.codec_name << " video decoder initialized";
    }

    void on_image_frame(const encoded_frame &f) {
      LOG(4) << this << " on_image_frame";
      tele::counter_inc(messages_received);
      tele::counter_inc(bytes_received, f.data.size());

      if (!_context) {
        tele::counter_inc(messages_dropped);
      }

      {
        stopwatch<> s;
        av_init_packet(_packet.get());
        _packet->flags |= f.key_frame ? AV_PKT_FLAG_KEY : 0;
        _packet->data = (uint8_t *)f.data.data();
        _packet->size = static_cast<int>(f.data.size());
        _packet->pos = f.id.i1;
        _packet->duration = f.id.i2 - f.id.i1;
        _packet->pts = _packet->dts =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                f.timestamp.time_since_epoch())
                .count();

        int err = avcodec_send_packet(_context.get(), _packet.get());
        av_packet_unref(_packet.get());
        if (err) {
          LOG(ERROR) << "avcodec_send_packet error: " << avutils::error_msg(err);
          return;
        }
        tele::distribution_add(send_packet_millis, s.millis());
      }
    }

    bool drain_impl() override {
      LOG(4) << this << " drain_impl needs=" << needs();
      if (!_context) {
        LOG(4) << this << " requesting metadata";
        _source->request(1);

        if (!_context) {
          LOG(4) << this << " context not ready";
          return false;
        }

        LOG(4) << this << " context ready";
      }

      auto ec = receive_frame();
      if (ec == video_error::FrameNotReadyError) {
        LOG(4) << this << " frame not ready, requesting next";
        _source->request(1);
      }
      return !static_cast<bool>(ec);
    }

    std::error_condition receive_frame() {
      LOG(4) << this << " receive_frame";

      stopwatch<> s;
      int err = avcodec_receive_frame(_context.get(), _frame.get());
      if (err) {
        switch (err) {
          case AVERROR(EAGAIN):
            LOG(4) << this << " eagain";
            return video_error::FrameNotReadyError;
          case AVERROR_EOF:
            LOG(4) << this << " eof";
            deliver_on_complete();
            return video_error::EndOfStreamError;
          default:
            LOG(ERROR) << "avcodec_receive_frame error: " << avutils::error_msg(err);
            deliver_on_error(video_error::FrameGenerationError);
            return video_error::FrameGenerationError;
        }
      }

      tele::distribution_add(receive_frame_millis, s.millis());
      deliver_frame();
      return {};
    }

    void deliver_frame() {
      if (!_image) {
        if (!init_image()) {
          deliver_on_error(video_error::StreamInitializationError);
          return;
        }
      }

      sws_scale(_sws_context.get(), _frame->data, _frame->linesize, 0, _frame->height,
                _image->data, _image->linesize);

      frame_id id{_frame->pkt_pos, _frame->pkt_pos + _frame->pkt_duration};

      owned_image_frame frame;
      frame.id = id;
      frame.pixel_format = _pixel_format;
      frame.width = static_cast<uint16_t>(_image_width);
      frame.height = static_cast<uint16_t>(_image_height);
      frame.timestamp =
          std::chrono::system_clock::time_point{std::chrono::milliseconds(_frame->pts)};

      for (uint8_t i = 0; i < MAX_IMAGE_PLANES; i++) {
        const uint32_t plane_stride = static_cast<uint32_t>(_image->linesize[i]);
        const uint8_t *plane_data = _image->data[i];
        frame.plane_strides[i] = plane_stride;
        if (plane_stride > 0) {
          frame.plane_data[i].assign(plane_data,
                                     plane_data + (plane_stride * _image_height));
        }
      }

      tele::counter_inc(frames_received);
      deliver_on_next(owned_image_packet{frame});
    }

    bool init_image() {
      _image_width =
          _bounding_width != ORIGINAL_IMAGE_WIDTH ? _bounding_width : _frame->width;
      _image_height =
          _bounding_height != ORIGINAL_IMAGE_HEIGHT ? _bounding_height : _frame->height;

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

      LOG(INFO) << "decoder resolution is " << _image_width << "x" << _image_height;

      _image = avutils::allocate_image(_image_width, _image_height, _pixel_format);
      if (!_image) {
        LOG(ERROR) << "allocate_image failed";
        return false;
      }

      _sws_context = avutils::sws_context(
          _frame->width, _frame->height, (AVPixelFormat)_frame->format, _image_width,
          _image_height, avutils::to_av_pixel_format(_pixel_format));

      if (!_sws_context) {
        LOG(ERROR) << "sws_context failed";
        return false;
      }

      return true;
    }

    const image_pixel_format _pixel_format;
    const int _bounding_width;
    const int _bounding_height;
    const bool _keep_proportions;
    streams::subscription *_source{nullptr};
    int _image_width{0};
    int _image_height{0};
    encoded_metadata _metadata;
    std::shared_ptr<AVCodecContext> _context;
    std::shared_ptr<AVPacket> _packet;
    std::shared_ptr<AVFrame> _frame;
    std::shared_ptr<avutils::allocated_image> _image;
    std::shared_ptr<SwsContext> _sws_context;
  };

  const image_pixel_format _pixel_format;
  const int _bounding_width;
  const int _bounding_height;
  const bool _keep_proportions;
};

}  // namespace

streams::op<encoded_packet, owned_image_packet> decode_image_frames(
    int bounding_width, int bounding_height, image_pixel_format pixel_format,
    bool keep_proportions) {
  avutils::init();

  return [bounding_width, bounding_height, pixel_format,
          keep_proportions](streams::publisher<encoded_packet> &&src) {
    return std::move(src) >> image_decoder_op(pixel_format, bounding_width,
                                              bounding_height, keep_proportions);
  };
}

}  // namespace video
}  // namespace satori