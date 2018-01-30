#include "video_streams.h"

#include <memory>

#include "avutils.h"
#include "data.h"
#include "logging.h"
#include "mkv_options.h"
#include "streams/streams.h"

// TODO: #1 convert to general video_file_sink to support mp4 and mkv at least
// TODO: #2 use AVMEDIA_TYPE_DATA or subtitles for annotations
// TODO: #3 maybe use a separate thread for file_writer
// TODO: #4 add --segment-frames parameter
namespace satori {
namespace video {
namespace {

class mkv_file_writer {
 public:
  mkv_file_writer(const std::string &filename, const mkv::format_options &format_options,
                  const encoded_metadata &metadata) {
    avutils::init();

    LOG(INFO) << "Creating format context for file " << filename;
    _format_context = avutils::output_format_context(
        "matroska", filename, [filename](AVFormatContext *ctx) {
          if (ctx->pb != nullptr) {
            LOG(INFO) << "Writing trailer section into file " << filename;
            av_write_trailer(ctx);
            LOG(INFO) << "Closing file " << filename;
            avio_closep(&ctx->pb);
          }
        });
    CHECK(_format_context) << "could not allocate format context for " << filename;

    LOG(INFO) << "Initializing matroska sink for file " << filename;
    auto encoder = reconstruct_encoder(metadata, filename);

    LOG(INFO) << "Creating video stream for file " << filename;
    AVStream *video_stream = avformat_new_stream(_format_context.get(), nullptr);
    CHECK_NOTNULL(video_stream) << "failed to create an output video stream";
    video_stream->id = _format_context->nb_streams - 1;
    video_stream->time_base = encoder->time_base;
    _video_stream_index = video_stream->index;

    LOG(INFO) << "Copying encoder parameters into video stream for file " << filename;
    int ret = avcodec_parameters_from_context(video_stream->codecpar, encoder.get());
    CHECK_GE(ret, 0) << "failed to copy codec parameters: " << avutils::error_msg(ret);

    LOG(INFO) << "Opening file " << filename;
    ret = avio_open(&_format_context->pb, filename.c_str(), AVIO_FLAG_WRITE);
    CHECK_GE(ret, 0) << "failed to open file: " << avutils::error_msg(ret);

    LOG(INFO) << "Writing header section into file " << filename;
    AVDictionary *options_dict{nullptr};
    av_dict_set(&options_dict, "reserve_index_space",
                std::to_string(format_options.reserved_index_space).c_str(), 0);
    ret = avformat_write_header(_format_context.get(), &options_dict);
    CHECK_GE(ret, 0) << "failed to write header: " << avutils::error_msg(ret);
    av_dict_free(&options_dict);
  }

  void write_frame(const encoded_frame &f) {
    if (_first_frame) {
      _start_ts = f.timestamp;
      _first_frame = false;
    }

    AVPacket packet{nullptr};
    av_init_packet(&packet);
    packet.data = (uint8_t *)f.data.data();
    packet.size = static_cast<int>(f.data.size());
    packet.pts = packet.dts =
        std::chrono::duration_cast<std::chrono::milliseconds>(f.timestamp - _start_ts)
            .count();
    if (f.key_frame) {
      LOG(INFO) << "got key frame";
      packet.flags |= AV_PKT_FLAG_KEY;
    }
    LOG(2) << "pts = " << packet.pts;
    packet.stream_index = _video_stream_index;
    int ret = av_interleaved_write_frame(_format_context.get(), &packet);
    CHECK_GE(ret, 0) << "failed to write packet: " << avutils::error_msg(ret);
    av_packet_unref(&packet);
  }

 private:
  std::shared_ptr<AVCodecContext> reconstruct_encoder(const encoded_metadata &metadata,
                                                      const std::string &filename) {
    CHECK(metadata.image_size);

    auto encoder = avutils::encoder_context(avutils::codec_id(metadata.codec_name));
    CHECK(encoder) << "could not allocate encoder context for " << filename;

    if ((_format_context->oformat->flags & AVFMT_GLOBALHEADER) != 0) {
      encoder->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    encoder->width = metadata.image_size->width;
    encoder->height = metadata.image_size->height;

    return encoder;
  }

  std::shared_ptr<AVFormatContext> _format_context{nullptr};
  int _video_stream_index{-1};
  bool _first_frame{true};
  std::chrono::system_clock::time_point _start_ts;
};

class mkv_sink_impl : public streams::subscriber<encoded_packet>,
                      boost::static_visitor<void> {
 public:
  mkv_sink_impl(const std::string &filename, const uint16_t segment_duration,
                const mkv::format_options &format_options)
      : _is_segmented{segment_duration > 0},
        _segment_duration{std::chrono::seconds{segment_duration}},
        _format_options{format_options} {
    const auto dot_pos = filename.rfind('.');
    if (dot_pos != std::string::npos) {
      _basename = filename.substr(0, dot_pos);
      _extension = filename.substr(dot_pos);
    } else {
      _basename = filename;
    }
  }

  void operator()(const encoded_metadata &metadata) {
    if (!_initialized) {
      _metadata = metadata;  // TODO: handle metadata change
      _initialized = true;
    }
  }

  void operator()(const encoded_frame &f) {
    if (!_initialized) {
      return;
    }

    if (!_is_segmented) {
      if (!_file_writer) {
        LOG(INFO) << "starting non-segmented file " << filename(nullptr, nullptr);
        _file_writer = std::make_unique<mkv_file_writer>(filename(nullptr, nullptr),
                                                         _format_options, _metadata);
      }

      _file_writer->write_frame(f);
      return;
    }

    if (f.key_frame) {
      if (_file_writer && f.timestamp >= _segment_start_ts + _segment_duration) {
        _file_writer.reset();
        const std::string old_name = filename(&_segment_start_ts, nullptr);
        const std::string new_name = filename(&_segment_start_ts, &_last_frame_ts);
        LOG(INFO) << "Renaming " << old_name << " to " << new_name << "...";
        const int ret = std::rename(old_name.c_str(), new_name.c_str());
        CHECK_EQ(ret, 0) << "Failed to rename " << old_name << " to " << new_name << ": "
                         << strerror(ret);
        LOG(INFO) << "Successfully renamed " << old_name << " to " << new_name << "...";
      }

      if (!_file_writer) {
        _segment_start_ts = f.timestamp;

        const auto name = filename(&_segment_start_ts, nullptr);
        LOG(INFO) << "starting new segment " << name;
        _file_writer =
            std::make_unique<mkv_file_writer>(name, _format_options, _metadata);
      }
    }

    if (_file_writer) {
      _file_writer->write_frame(f);
      _last_frame_ts = f.timestamp;
    }
  }

 private:
  std::string filename(std::chrono::system_clock::time_point *start,
                       std::chrono::system_clock::time_point *end) {
    if (start == nullptr) {
      return _basename + _extension;
    }
    CHECK_NOTNULL(start);

    if (end == nullptr) {
      return _basename + "-" + std::to_string(start->time_since_epoch().count())
             + _extension;
    }

    return _basename + "-" + std::to_string(start->time_since_epoch().count()) + "-"
           + std::to_string(end->time_since_epoch().count()) + _extension;
  }

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

  std::string _basename;
  std::string _extension;
  const bool _is_segmented;
  const std::chrono::system_clock::duration _segment_duration;
  const mkv::format_options _format_options;
  bool _initialized{false};
  encoded_metadata _metadata;
  std::unique_ptr<mkv_file_writer> _file_writer{nullptr};
  std::chrono::system_clock::time_point _segment_start_ts;
  std::chrono::system_clock::time_point _last_frame_ts;
  streams::subscription *_src{nullptr};
};

}  // namespace

streams::subscriber<encoded_packet> &mkv_sink(const std::string &filename,
                                              const uint16_t segment_duration,
                                              const mkv::format_options &format_options) {
  return *(new mkv_sink_impl(filename, segment_duration, format_options));
}

}  // namespace video
}  // namespace satori