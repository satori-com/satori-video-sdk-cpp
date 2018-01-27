#include <stdexcept>

#include "avutils.h"
#include "data.h"
#include "logging.h"
#include "mkv_options.h"
#include "streams/streams.h"
#include "video_streams.h"

namespace satori {
namespace video {

// TODO: use AVMEDIA_TYPE_DATA
class mkv_sink_impl : public streams::subscriber<encoded_packet>,
                      boost::static_visitor<void> {
 public:
  mkv_sink_impl(const std::string &filename, const mkv::format_options &format_options)
      : _filename(filename), _format_options(format_options) {
    avutils::init();

    _format_context = avutils::output_format_context(
        "matroska", _filename, [filename](AVFormatContext *ctx) {
          if (ctx->pb != nullptr) {
            LOG(INFO) << "Writing trailer section into file " << filename;
            av_write_trailer(ctx);
            LOG(INFO) << "Closing file " << filename;
            avio_closep(&ctx->pb);
          }
        });
    if (_format_context == nullptr) {
      throw std::runtime_error{"could not allocate format context for " + _filename};
    }
  }

  void operator()(const encoded_metadata &metadata) {
    CHECK(metadata.image_size);

    if (!_initialized) {
      LOG(INFO) << "Initializing matroska sink for file " << _filename;
      std::shared_ptr<AVCodecContext> encoder_context =
          avutils::encoder_context(avutils::codec_id(metadata.codec_name));
      if (encoder_context == nullptr) {
        throw std::runtime_error{"could not allocate encoder context for " + _filename};
      }
      if ((_format_context->oformat->flags & AVFMT_GLOBALHEADER) != 0) {
        encoder_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
      }
      encoder_context->width = metadata.image_size->width;
      encoder_context->height = metadata.image_size->height;

      LOG(INFO) << "Creating video stream for file " << _filename;
      AVStream *video_stream = avformat_new_stream(_format_context.get(), nullptr);
      if (video_stream == nullptr) {
        throw std::runtime_error{"failed to create an output video stream"};
      }
      video_stream->id = _format_context->nb_streams - 1;
      video_stream->time_base = encoder_context->time_base;
      _video_stream_index = video_stream->index;

      LOG(INFO) << "Copying encoder parameters into video stream for file " << _filename;
      int ret =
          avcodec_parameters_from_context(video_stream->codecpar, encoder_context.get());
      if (ret < 0) {
        throw std::runtime_error{"failed to copy codec parameters: "
                                 + avutils::error_msg(ret)};
      }

      LOG(INFO) << "Opening file " << _filename;
      ret = avio_open(&_format_context->pb, _filename.c_str(), AVIO_FLAG_WRITE);
      if (ret < 0) {
        throw std::runtime_error{"failed to open file: " + avutils::error_msg(ret)};
      }

      LOG(INFO) << "Writing header section into file " << _filename;
      AVDictionary *format_options{nullptr};
      av_dict_set(&format_options, "reserve_index_space",
                  std::to_string(_format_options.reserved_index_space).c_str(), 0);
      ret = avformat_write_header(_format_context.get(), &format_options);
      av_dict_free(&format_options);
      if (ret < 0) {
        throw std::runtime_error{"failed to write header: " + avutils::error_msg(ret)};
      }

      _initialized = true;
    }
  }

  void operator()(const encoded_frame &f) {
    if (!_initialized) {
      return;
    }

    if (!_first_frame_ts) {
      _first_frame_ts = f.timestamp;
    }

    AVPacket packet;
    av_init_packet(&packet);
    packet.data = (uint8_t *)f.data.data();
    packet.size = f.data.size();
    packet.pts = packet.dts = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  f.timestamp - *_first_frame_ts)
                                  .count();
    if (f.key_frame) {
      LOG(INFO) << "got key frame";
      packet.flags |= AV_PKT_FLAG_KEY;
    }
    LOG(2) << "pts = " << packet.pts;
    packet.stream_index = _video_stream_index;
    int ret = av_interleaved_write_frame(_format_context.get(), &packet);
    if (ret < 0) {
      throw std::runtime_error{"failed to write packet: " + avutils::error_msg(ret)};
    }
    av_packet_unref(&packet);
  }

 private:
  void on_next(encoded_packet &&packet) override {
    boost::apply_visitor(*this, packet);
    _src->request(1);
  }

  void on_error(std::error_condition ec) override { ABORT() << ec.message(); }

  void on_complete() override {
    LOG(INFO) << "got complete";
    delete this;
  }

  void on_subscribe(streams::subscription &s) override {
    _src = &s;
    _src->request(1);
  }

  streams::subscription *_src;
  const std::string _filename;
  const mkv::format_options _format_options;
  std::shared_ptr<AVFormatContext> _format_context{nullptr};
  bool _initialized{false};
  int _video_stream_index{-1};
  boost::optional<std::chrono::system_clock::time_point> _first_frame_ts;
};

streams::subscriber<encoded_packet> &mkv_sink(const std::string &filename,
                                              const mkv::format_options &format_options) {
  return *(new mkv_sink_impl(filename, format_options));
}

}  // namespace video
}  // namespace satori