#include <cstring>
#include <gsl/gsl>
#include <memory>
#include <string>
#include <thread>

extern "C" {
#include <libavutil/display.h>
#include <libavutil/error.h>
}

#include "avutils.h"
#include "logging.h"
#include "streams/asio_streams.h"
#include "video_error.h"
#include "video_streams.h"

namespace satori {
namespace video {
namespace {

class file_source_impl {
 public:
  file_source_impl(const std::string &filename, const bool loop)
      : _filename(filename), _loop(loop), _start(std::chrono::system_clock::now()) {}

  std::error_condition init() {
    LOG(1) << "Opening file " << _filename;

    _fmt_ctx = avutils::open_input_format_context(_filename);
    if (!_fmt_ctx) {
      return video_error::STREAM_INITIALIZATION_ERROR;
    }

    LOG(1) << "File " << _filename << " is open";

    _stream_idx = avutils::find_best_video_stream(_fmt_ctx.get(), &_dec);
    if (_stream_idx < 0) {
      return video_error::STREAM_INITIALIZATION_ERROR;
    }
    _stream = _fmt_ctx->streams[_stream_idx];

    LOG(1) << "Allocating codec context...";
    _dec_ctx = avutils::decoder_context(_dec);
    if (!_dec_ctx) {
      return video_error::STREAM_INITIALIZATION_ERROR;
    }
    LOG(1) << "Codec context is allocated";

    int ret = 0;

    LOG(1) << "Copying codec parameters to codec context...";
    if ((ret = avcodec_parameters_to_context(_dec_ctx.get(), _stream->codecpar)) < 0) {
      LOG(ERROR) << "Failed to copy codec parameters to codec context:"
                 << avutils::error_msg(ret);
      return video_error::STREAM_INITIALIZATION_ERROR;
    }
    LOG(1) << "Codec parameters were copied to codec context";

    LOG(1) << "Opening video codec...";
    AVDictionary *opts = nullptr;
    if ((ret = avcodec_open2(_dec_ctx.get(), _dec, &opts)) < 0) {
      LOG(ERROR) << "*** Failed to open video codec:" << avutils::error_msg(ret);
      if (ret == AVERROR_EOF) {
        return video_error::END_OF_STREAM_ERROR;
      }
      return video_error::STREAM_INITIALIZATION_ERROR;
    }
    LOG(1) << "Video codec is open";
    return {};
  }

  void generate_one(streams::observer<encoded_packet> &observer) {
    if (_fmt_ctx == nullptr) {
      if (auto err = init()) {
        if (err == video_error::END_OF_STREAM_ERROR) {
          observer.on_complete();
        } else {
          observer.on_error(err);
        }
        return;
      }
    }

    if (!_metadata_sent) {
      send_metadata(observer);
      return;
    }

    av_init_packet(&_pkt);
    auto release = gsl::finally([this]() { av_packet_unref(&_pkt); });

    int ret = av_read_frame(_fmt_ctx.get(), &_pkt);
    if (ret < 0) {
      if (ret == AVERROR_EOF) {
        if (_loop) {
          LOG(4) << "restarting " << _filename;
          av_seek_frame(_fmt_ctx.get(), _stream_idx, _fmt_ctx->start_time,
                        AVSEEK_FLAG_BACKWARD);
          return;
        }

        LOG(4) << "eof in " << _filename;
        observer.on_complete();
        return;
      }

      observer.on_error(video_error::FRAME_GENERATION_ERROR);
      return;
    }

    if (_pkt.stream_index == _stream_idx) {
      LOG(4) << "packet from file " << _filename;
      encoded_frame frame;
      frame.data = std::string{_pkt.data, _pkt.data + _pkt.size};
      _last_pos++;
      frame.id = {_last_pos, _last_pos};
      auto ts = 1000 * _pkt.pts * _stream->time_base.num / _stream->time_base.den;
      frame.timestamp = _start + std::chrono::milliseconds(ts);
      frame.creation_time = std::chrono::system_clock::now();
      frame.key_frame = static_cast<bool>(_pkt.flags & AV_PKT_FLAG_KEY);
      observer.on_next(frame);
    }
  }

  void send_metadata(streams::observer<encoded_packet> &observer) {
    encoded_metadata metadata;
    metadata.codec_name = _dec->name;
    metadata.codec_data.assign(_dec_ctx->extradata,
                               _dec_ctx->extradata + _dec_ctx->extradata_size);
    auto display_rotation = get_display_rotation();
    if (display_rotation.is_initialized()) {
      metadata.additional_data = nlohmann::json::object();
      metadata.additional_data["display_rotation"] = display_rotation.get();
    }
    observer.on_next(std::move(metadata));
    _metadata_sent = true;
  }

 private:
  // based on FFmpeg's get_rotation() function
  boost::optional<double> get_display_rotation() const {
    uint8_t *display_matrix =
        av_stream_get_side_data(_stream, AV_PKT_DATA_DISPLAYMATRIX, nullptr);
    if (display_matrix == nullptr) {
      return {};
    }

    // Returns angle in [-180.0; 180.0] range
    double theta =
        -av_display_rotation_get(reinterpret_cast<const int32_t *>(display_matrix));

    // Converting angle to [0.0; 360.0] range
    theta -= 360 * std::floor(theta / 360 + 0.9 / 360);

    CHECK_LE(std::abs(theta - 90 * std::round(theta / 90)), 2);
    return theta;
  }

  const std::string _filename;
  const bool _loop{false};

  std::chrono::system_clock::time_point _start;
  std::shared_ptr<AVFormatContext> _fmt_ctx;
  int _stream_idx{-1};
  AVStream *_stream{nullptr};
  AVCodec *_dec{nullptr};  // TODO: check if possible to destroy it
  std::shared_ptr<AVCodecContext> _dec_ctx;
  AVPacket _pkt{nullptr};
  int64_t _last_pos{0};
  bool _metadata_sent{false};
};

double get_fps(const std::string &filename) {
  LOG(INFO) << "Reading fps from " << filename;

  std::shared_ptr<AVFormatContext> format_context =
      avutils::open_input_format_context(filename);
  CHECK(format_context) << "Unable to open file " << filename;

  const int stream_index = avutils::find_best_video_stream(format_context.get(), nullptr);
  CHECK_GE(stream_index, 0) << "Unable to find video stream in file " << filename;
  const AVStream *stream = format_context->streams[stream_index];

  const double fps =
      static_cast<double>(stream->avg_frame_rate.num) / stream->avg_frame_rate.den;

  CHECK_GT(fps, 0.0) << "Invalid fps for file " << filename;

  LOG(INFO) << "fps=" << fps << " for file " << filename;

  return fps;
}

}  // namespace

streams::publisher<encoded_packet> file_source(boost::asio::io_service &io,
                                               const std::string &filename, bool loop,
                                               bool batch) {
  avutils::init();
  streams::publisher<encoded_packet> result =
      streams::generators<encoded_packet>::stateful(
          [filename, loop]() { return new file_source_impl(filename, loop); },
          [](file_source_impl *impl, streams::observer<encoded_packet> &sink) {
            impl->generate_one(sink);
          });

  if (!batch) {
    const double fps = get_fps(filename);
    result = std::move(result)
             >> streams::asio::interval<encoded_packet>(
                    io, std::chrono::milliseconds(static_cast<long>(1000 / fps)));
  }

  return std::move(result) >> repeat_metadata();
}

}  // namespace video
}  // namespace satori
