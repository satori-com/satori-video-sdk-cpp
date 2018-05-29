#include "video_streams.h"

#include <boost/filesystem.hpp>
#include <memory>

#include "avutils.h"
#include "data.h"
#include "logging.h"
#include "streams/streams.h"

// TODO: * use AVMEDIA_TYPE_DATA or subtitles for annotations
// TODO: * maybe use a separate thread for file_writer
// TODO: * add --segment-frames parameter
namespace satori {
namespace video {
namespace {

namespace fs = boost::filesystem;

constexpr AVRational milliseconds_time_base = {1, 1000};

fs::path temp_dir(const fs::path &work_path) {
  CHECK(work_path.has_extension());

  auto result = work_path.parent_path() / "temp-recordings";

  boost::system::error_code ec;
  fs::create_directory(result, ec);
  CHECK_EQ(ec.value(), 0) << "failed to create temporary directory: " << ec.message();

  return result;
}

fs::path temp_file_template(const fs::path &temp_dir, const fs::path &extension) {
  auto result = temp_dir / "%%%%-%%%%-%%%%-%%%%";
  result += extension;

  return result;
}

// decodes stream to extract information about image sizes, etc.
class stream_decoder {
 public:
  stream_decoder(const encoded_metadata &metadata) : _metadata{metadata} {
    avutils::init();
    _packet = avutils::av_packet();
    _frame = avutils::av_frame();
    _context = avutils::decoder_context(_metadata.codec_name, _metadata.codec_data);
  }

  void feed(const encoded_frame &f) {
    if (_stream_image_size) {
      LOG(4) << "already has stream size " << *_stream_image_size;
      return;
    }

    av_init_packet(_packet.get());
    _packet->data = (uint8_t *)f.data.data();
    _packet->size = static_cast<int>(f.data.size());
    _packet->flags |= f.key_frame ? AV_PKT_FLAG_KEY : 0;
    LOG(4) << "sending a packet";
    int ret = avcodec_send_packet(_context.get(), _packet.get());
    if (ret < 0) {
      LOG(ERROR) << "avcodec_send_packet error: " << avutils::error_msg(ret);
    }
    av_packet_unref(_packet.get());

    LOG(4) << "trying to receive a frame";
    ret = avcodec_receive_frame(_context.get(), _frame.get());
    if (ret >= 0) {
      LOG(INFO) << "stream resolution is " << _frame->width << "x" << _frame->height;
      _stream_image_size = image_size{static_cast<int16_t>(_frame->width),
                                      static_cast<int16_t>(_frame->height)};
    } else {
      LOG(1) << "avcodec_receive_frame error: " << avutils::error_msg(ret);
    }
  }

  const encoded_metadata &metadata() const { return _metadata; }

  const boost::optional<image_size> &stream_image_size() const {
    return _stream_image_size;
  }

 private:
  const encoded_metadata _metadata;
  std::shared_ptr<AVPacket> _packet;
  std::shared_ptr<AVFrame> _frame;
  std::shared_ptr<AVCodecContext> _context;
  boost::optional<image_size> _stream_image_size;
};

AVStream *create_video_stream(AVFormatContext &format_context,
                              const stream_decoder &decoder) {
  CHECK(decoder.stream_image_size());
  AVStream *video_stream = avformat_new_stream(&format_context, nullptr);
  CHECK_NOTNULL(video_stream) << "failed to create an output video stream";

  video_stream->id = format_context.nb_streams - 1;
  video_stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
  video_stream->codecpar->codec_id = avutils::codec_id(decoder.metadata().codec_name);
  video_stream->codecpar->width = decoder.stream_image_size()->width;
  video_stream->codecpar->height = decoder.stream_image_size()->height;

  const auto &codec_data = decoder.metadata().codec_data;

  video_stream->codecpar->extradata_size = codec_data.size();
  auto buffer =
      reinterpret_cast<uint8_t *>(av_memdup(codec_data.data(), codec_data.size()));
  CHECK(buffer);
  video_stream->codecpar->extradata = buffer;

  return video_stream;
}

// TODO: maybe add a check for supported codecs and containers
class video_file_writer {
 public:
  video_file_writer(const fs::path &filename, const stream_decoder &decoder,
                    const std::unordered_map<std::string, std::string> &options)
      : _filename{filename} {
    avutils::init();

    LOG(INFO) << "Creating format context for file " << _filename;
    _format_context =
        avutils::output_format_context("", _filename.string(), [](AVFormatContext *ctx) {
          if (ctx->pb != nullptr) {
            LOG(INFO) << "Writing trailer section into file " << ctx->filename;
            av_write_trailer(ctx);
            LOG(INFO) << "Closing file " << ctx->filename;
            avio_closep(&ctx->pb);
          }
        });
    CHECK(_format_context) << "could not allocate format context for " << _filename;

    LOG(INFO) << "Creating video stream for file " << _filename;
    _video_stream = create_video_stream(*_format_context, decoder);

    LOG(INFO) << "Opening file " << _filename;
    int ret = avio_open(&_format_context->pb, _filename.c_str(), AVIO_FLAG_WRITE);
    CHECK_GE(ret, 0) << "failed to open file: " << avutils::error_msg(ret);

    LOG(INFO) << "Writing header section into file " << _filename;
    AVDictionary *options_dict{nullptr};
    for (auto kv : options) {
      LOG(2) << "Adding container option: {" << kv.first << "," << kv.second << "}";
      av_dict_set(&options_dict, kv.first.c_str(), kv.second.c_str(), 0);
    }
    ret = avformat_write_header(_format_context.get(), &options_dict);
    CHECK_GE(ret, 0) << "failed to write header: " << avutils::error_msg(ret);
    av_dict_free(&options_dict);
  }

  void write_frame(const encoded_frame &f) {
    if (!_started_processing) {
      _started_processing = true;
      _start_ts = f.timestamp;
    }
    _last_ts = f.timestamp;

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
    av_packet_rescale_ts(&packet, milliseconds_time_base, _video_stream->time_base);
    packet.stream_index = _video_stream->index;
    int ret = av_interleaved_write_frame(_format_context.get(), &packet);
    CHECK_GE(ret, 0) << "failed to write packet: " << avutils::error_msg(ret);
    av_packet_unref(&packet);
  }

  fs::path filename() const { return _filename; }

  std::chrono::system_clock::time_point start_ts() const {
    CHECK(_started_processing);
    return _start_ts;
  }

  std::chrono::system_clock::time_point last_ts() const {
    CHECK(_started_processing);
    return _last_ts;
  }

 private:
  const fs::path _filename;
  std::shared_ptr<AVFormatContext> _format_context{nullptr};
  AVStream *_video_stream{nullptr};
  bool _started_processing{false};
  std::chrono::system_clock::time_point _start_ts;
  std::chrono::system_clock::time_point _last_ts;
};

class video_file_sink_impl : public streams::subscriber<encoded_packet>,
                             boost::static_visitor<void> {
 public:
  video_file_sink_impl(
      const fs::path &path,
      const boost::optional<std::chrono::system_clock::duration> &segment_duration,
      std::unordered_map<std::string, std::string> &&options)
      : _path{path},
        _temp_file_template{temp_file_template(temp_dir(path), path.extension())},
        _segment_duration{segment_duration},
        _options{std::move(options)} {}

  ~video_file_sink_impl() override {
    if (_file_writer) {
      release_writer();
    }
  }

  void operator()(const encoded_metadata &metadata) {
    if (_decoder) {
      LOG(1) << "ignoring metadata";
      return;
    }
    _decoder = std::make_unique<stream_decoder>(metadata);
  }

  void operator()(const encoded_frame &f) {
    LOG(4) << "got encoded frame of size " << f.data.size();
    if (!_decoder) {
      LOG(4) << "no stream decoder";
      return;
    }
    if (!_decoder->stream_image_size()) {
      LOG(4) << "feeding stream decoder";
      _decoder->feed(f);
    }
    if (!_decoder->stream_image_size()) {
      LOG(4) << "stream decoder doesn't have image size";
      return;
    }

    if (f.key_frame) {
      if (_segment_duration && _file_writer
          && f.timestamp >= _file_writer->start_ts() + *_segment_duration) {
        release_writer();
      }

      if (!_file_writer) {
        _file_writer =
            std::make_unique<video_file_writer>(temp_filename(), *_decoder, _options);
        LOG(INFO) << "started new file " << _file_writer->filename();
      }
    }

    if (_file_writer) {
      _file_writer->write_frame(f);
    }
  }

 private:
  void release_writer() {
    const fs::path old_name = _file_writer->filename();
    const fs::path new_name = current_filename();
    _file_writer.reset();

    boost::system::error_code ec;
    fs::rename(old_name, new_name, ec);
    CHECK_EQ(ec.value(), 0) << "Failed to rename " << old_name << " to " << new_name
                            << ": " << ec.message();
    LOG(INFO) << "Successfully renamed " << old_name << " to " << new_name;
  }

  fs::path current_filename() const {
    if (!_segment_duration) {
      return _path.string();
    }

    const auto start_epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    _file_writer->start_ts().time_since_epoch())
                                    .count();
    const auto end_epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  _file_writer->last_ts().time_since_epoch())
                                  .count();
    fs::path result{_path.stem()};
    result += "-";
    result += std::to_string(start_epoch_ms);
    result += "-";
    result += std::to_string(end_epoch_ms);
    result += _path.extension();
    return _path.parent_path() / result;
  }

  fs::path temp_filename() const {
    boost::system::error_code ec;
    auto result = fs::unique_path(_temp_file_template, ec);
    CHECK_EQ(ec.value(), 0) << "Failed to generate temporary filename: " << ec.message();
    return result;
  }

  // TODO: propagate error down the stream if encoder is not supported
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

  const fs::path _path;
  const fs::path _temp_file_template;
  const boost::optional<std::chrono::system_clock::duration> _segment_duration;
  const std::unordered_map<std::string, std::string> _options;
  std::unique_ptr<stream_decoder> _decoder{nullptr};
  std::unique_ptr<video_file_writer> _file_writer{nullptr};
  streams::subscription *_src{nullptr};
};

}  // namespace

streams::subscriber<encoded_packet> &video_file_sink(
    const fs::path &path,
    const boost::optional<std::chrono::system_clock::duration> &segment_duration,
    std::unordered_map<std::string, std::string> &&options) {
  return *(new video_file_sink_impl(path, segment_duration, std::move(options)));
}

}  // namespace video
}  // namespace satori
