#include "video_streams.h"

#include <sstream>

#include "av_filter.h"
#include "avutils.h"
#include "metrics.h"
#include "stopwatch.h"
#include "video_error.h"

namespace satori {
namespace video {

namespace {

auto &frames_received = prometheus::BuildCounter()
                            .Name("decoder_frames_received_total")
                            .Register(metrics_registry())
                            .Add({});
auto &messages_received = prometheus::BuildCounter()
                              .Name("decoder_messages_received_total")
                              .Register(metrics_registry())
                              .Add({});
auto &messages_dropped = prometheus::BuildCounter()
                             .Name("decoder_messages_dropped_total")
                             .Register(metrics_registry())
                             .Add({});
auto &bytes_received = prometheus::BuildCounter()
                           .Name("decoder_bytes_received_total")
                           .Register(metrics_registry())
                           .Add({});

auto &send_packet_millis =
    prometheus::BuildHistogram()
        .Name("decoder_send_packet_millis")
        .Register(metrics_registry())
        .Add({}, std::vector<double>{0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1, 2,
                                     5, 10, 20, 50, 100});
auto &receive_frame_millis =
    prometheus::BuildHistogram()
        .Name("decoder_receive_frame_millis")
        .Register(metrics_registry())
        .Add({}, std::vector<double>{0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1, 2,
                                     5, 10, 20, 50, 100});

auto &decoder_errors =
    prometheus::BuildCounter().Name("decoder_errors_total").Register(metrics_registry());

class image_decoder_op {
 public:
  image_decoder_op(const image_size &bounding_size, image_pixel_format pixel_format,
                   bool keep_aspect_ratio)
      : _bounding_size{bounding_size},
        _pixel_format{pixel_format},
        _keep_aspect_ratio{keep_aspect_ratio} {}

  template <typename T>
  class instance : public streams::subscriber<encoded_packet>,
                   streams::impl::drain_source_impl<owned_image_packet>,
                   boost::static_visitor<void> {
    static_assert(std::is_same<T, encoded_packet>::value, "types mismatch");
    using value_t = owned_image_packet;

   public:
    static streams::publisher<owned_image_packet> apply(
        streams::publisher<encoded_packet> &&source, image_decoder_op &&op) {
      return streams::publisher<owned_image_packet>(
          new streams::impl::op_publisher<T, owned_image_packet, image_decoder_op>(
              std::move(source), op));
    }

    instance(image_decoder_op &&op, streams::subscriber<owned_image_packet> &sink)
        : streams::impl::drain_source_impl<owned_image_packet>(sink),
          _bounding_size{op._bounding_size},
          _pixel_format{op._pixel_format},
          _keep_aspect_ratio{op._keep_aspect_ratio} {}

    ~instance() override {
      if (_source) {
        _source->cancel();
      }
    }

   private:
    void on_subscribe(streams::subscription &s) override {
      LOG(4) << "on_subscribe";
      _source = &s;
      deliver_on_subscribe();
    }

    void on_next(encoded_packet &&pkt) override {
      LOG(4) << this << " on_next";
      boost::apply_visitor(*this, pkt);
      drain();
    }

    void on_complete() override {
      LOG(4) << this << " on_complete";
      _source = nullptr;
      if (_context) {
        int err = avcodec_send_packet(_context.get(), nullptr);
        if (err < 0) {
          LOG(ERROR) << "avcodec_send_packet final error: " << avutils::error_msg(err);
          decoder_errors
              .Add({{"err", std::to_string(err)}, {"call", "avcodec_send_packet"}})
              .Increment();
          deliver_on_error(video_error::FRAME_GENERATION_ERROR);
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

   public:
    void operator()(const encoded_metadata &m) {
      LOG(INFO) << this << " received stream metadata " << m;
      if (m.codec_data == _metadata.codec_data && m.codec_name == _metadata.codec_name) {
        LOG(INFO) << "Ignoring same metadata";
        return;
      }

      _current_metadata_frames_counter = 0;
      _metadata = m;
      _context = avutils::decoder_context(m.codec_name, m.codec_data);
      _packet = avutils::av_packet();
      _frame = avutils::av_frame();
      _filtered_frame = avutils::av_frame();
      if (!_context || !_packet || !_frame || !_filtered_frame) {
        deliver_on_error(video_error::STREAM_INITIALIZATION_ERROR);
        return;
      }

      LOG(INFO) << _metadata.codec_name << " video decoder initialized";
    }

    void operator()(const encoded_frame &f) {
      LOG(4) << this << " on_image_frame";
      messages_received.Increment();
      bytes_received.Increment(f.data.size());

      if (!_context) {
        LOG(WARNING) << "dropping frame because there is no codec context";
        messages_dropped.Increment();
        return;
      }

      {
        stopwatch<> s;
        av_init_packet(_packet.get());
        _ids.push(f.id);
        _packet->flags |= f.key_frame ? AV_PKT_FLAG_KEY : 0;
        _packet->data = (uint8_t *)f.data.data();
        _packet->size = static_cast<int>(f.data.size());
        _packet->pos = f.id.i1;
        _packet->duration = f.id.i2 - f.id.i1;
        _packet->pts = _packet->dts =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                f.timestamp.time_since_epoch())
                .count();

        if (_current_metadata_frames_counter % 1000 == 0) {
          LOG(INFO) << "CURRENT metadata is " << _metadata
                    << ", frames_counter=" << _current_metadata_frames_counter;
        }
        _current_metadata_frames_counter++;
        // TODO: wrap avcodec_send_packet() into C++ function that returns error_condition
        int err = avcodec_send_packet(_context.get(), _packet.get());
        av_packet_unref(_packet.get());
        if (err < 0) {
          LOG(ERROR) << "avcodec_send_packet error: " << avutils::error_msg(err);
          decoder_errors
              .Add({{"err", std::to_string(err)}, {"call", "avcodec_send_packet"}})
              .Increment();
          return;
        }
        send_packet_millis.Observe(s.millis());
      }
    }

   private:
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
      if (ec == video_error::FRAME_NOT_READY_ERROR) {
        LOG(4) << this << " frame not ready, requesting next";
        _source->request(1);
      }
      return !static_cast<bool>(ec);
    }

    std::error_condition receive_frame() {
      LOG(4) << this << " receive_frame";

      stopwatch<> s;
      int err = avcodec_receive_frame(_context.get(), _frame.get());
      if (err < 0) {
        switch (err) {
          case AVERROR(EAGAIN):
            LOG(4) << this << " eagain";
            return video_error::FRAME_NOT_READY_ERROR;
          case AVERROR_EOF:
            LOG(4) << this << " eof";
            deliver_on_complete();
            return video_error::END_OF_STREAM_ERROR;
          default:
            LOG(ERROR) << "avcodec_receive_frame error: " << avutils::error_msg(err);
            decoder_errors
                .Add({{"err", std::to_string(err)}, {"call", "avcodec_receive_frame"}})
                .Increment();
            deliver_on_error(video_error::FRAME_GENERATION_ERROR);
            return video_error::FRAME_GENERATION_ERROR;
        }
      }
      receive_frame_millis.Observe(s.millis());
      deliver_frame();
      return {};
    }

    void deliver_frame() {
      if (!_filter) {
        init_filter();
      }

      _filter->feed(*_frame);
      frames_received.Increment();

      while (_filter->try_retrieve(*_filtered_frame)) {
        owned_image_frame frame = avutils::to_image_frame(*_filtered_frame);

        if (!_ids.empty()) {
          frame.id = _ids.front();
          _ids.pop();
        } else {
          LOG(ERROR) << this << "id queue is empty";
          frame.id = {_filtered_frame->pkt_pos,
                      _filtered_frame->pkt_pos + _filtered_frame->pkt_duration};
        }

        while (_filtered_frame->key_frame != 0 && _filtered_frame->pkt_pos != frame.id.i1
               && !_ids.empty()) {
          frame.id = _ids.front();
          _ids.pop();
        }

        av_frame_unref(_filtered_frame.get());
        deliver_on_next(owned_image_packet{std::move(frame)});
      }
    }

    void init_filter() {
      std::ostringstream filter_buffer;

      const auto &additional = _metadata.additional_data;
      if (additional.is_object()
          && additional.find("display_rotation") != additional.end()) {
        const double display_rotation = additional["display_rotation"];
        LOG(INFO) << "display rotation angle " << display_rotation;

        if (std::abs(display_rotation - 90) < 1.0) {
          filter_buffer << "transpose=clock";
        } else if (std::abs(display_rotation - 180) < 1.0) {
          filter_buffer << "hflip,vflip";
        } else if (std::abs(display_rotation - 270) < 1.0) {
          filter_buffer << "transpose=cclock";
        } else if (std::abs(display_rotation) > 1.0) {
          // TODO: floating point formatting?
          filter_buffer << "rotate=" << display_rotation << "*PI/180";
        }
      }

      if (filter_buffer.tellp() > 0) {
        filter_buffer << ",";
      }
      filter_buffer << "scale=";
      filter_buffer << "w=" << _bounding_size.width << ":h=" << _bounding_size.height;
      if (_keep_aspect_ratio) {
        filter_buffer << ":force_original_aspect_ratio=decrease";
      }

      CHECK_GT(filter_buffer.tellp(), 0);
      const std::string filter_string = filter_buffer.str();
      LOG(INFO) << "got a filter: " << filter_string;

      _filter = std::make_unique<av_filter>(filter_string, *_frame, _context->time_base,
                                            _pixel_format);
    }

    const image_size _bounding_size;
    const image_pixel_format _pixel_format;
    const bool _keep_aspect_ratio;
    streams::subscription *_source{nullptr};
    uint64_t _current_metadata_frames_counter{0};
    encoded_metadata _metadata;
    std::shared_ptr<AVCodecContext> _context;
    std::shared_ptr<AVPacket> _packet;
    std::shared_ptr<AVFrame> _frame;
    std::shared_ptr<AVFrame> _filtered_frame;
    std::unique_ptr<av_filter> _filter;
    std::queue<frame_id> _ids;
  };

 private:
  const image_size _bounding_size;
  const image_pixel_format _pixel_format;
  const bool _keep_aspect_ratio;
};

}  // namespace

streams::op<encoded_packet, owned_image_packet> decode_image_frames(
    const image_size &bounding_size, image_pixel_format pixel_format,
    bool keep_aspect_ratio) {
  avutils::init();

  return [bounding_size, pixel_format,
          keep_aspect_ratio](streams::publisher<encoded_packet> &&src) {
    return std::move(src)
           >> image_decoder_op(bounding_size, pixel_format, keep_aspect_ratio);
  };
}

}  // namespace video
}  // namespace satori
