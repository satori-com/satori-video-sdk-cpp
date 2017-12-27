#include <thread>
#include "avutils.h"
#include "metrics.h"
#include "threadutils.h"
#include "video_error.h"
#include "video_streams.h"

extern "C" {
#include <libavformat/avformat.h>
}

namespace satori {
namespace video {

namespace {
auto &frames_total = prometheus::BuildCounter()
                         .Name("url_source_frames_total")
                         .Register(metrics_registry());
}

class url_source_impl {
 public:
  url_source_impl(const std::string &url, const std::string &options,
                  streams::observer<encoded_packet> &sink)
      : _url(url), _sink(sink) {
    avutils::init();
    std::thread([this, options]() {
      threadutils::set_current_thread_name("read-loop");

      _reader_thread_id = std::this_thread::get_id();

      std::error_condition ec = start(options);
      if (ec) {
        _sink.on_error(ec);
        return;
      }

      read_loop();
    })
        .detach();
  }

  void stop() {
    if (_reader_thread_id == std::this_thread::get_id()) {
      delete this;
    } else {
      ABORT_S() << "should not happen";
    }
  }

  std::error_condition start(const std::string &options) {
    AVDictionary *options_dict = nullptr;
    int err = av_dict_parse_string(&options_dict, options.c_str(), "=", ";", 0);
    if (err < 0) {
      LOG(ERROR) << "can't parse options: " << options;
      return video_error::STREAM_INITIALIZATION_ERROR;
    }
    _input_context = avutils::open_input_format_context(_url, nullptr, options_dict);
    if (!_input_context) {
      return video_error::STREAM_INITIALIZATION_ERROR;
    }

    _stream_idx = avutils::find_best_video_stream(_input_context.get(), &_decoder);
    if (_stream_idx < 0) {
      return video_error::STREAM_INITIALIZATION_ERROR;
    }
    AVStream *stream = _input_context->streams[_stream_idx];
    _time_base = stream->time_base;

    av_read_play(_input_context.get());

    _decoder_context = avutils::decoder_context(_decoder);
    if (!_decoder_context) {
      return video_error::STREAM_INITIALIZATION_ERROR;
    }

    int ret = avcodec_parameters_to_context(_decoder_context.get(), stream->codecpar);
    if (ret < 0) {
      LOG(ERROR) << "failed to copy codec parameters to codec context:"
                 << avutils::error_msg(ret);
      return video_error::STREAM_INITIALIZATION_ERROR;
    }

    AVDictionary *opts = nullptr;
    ret = avcodec_open2(_decoder_context.get(), _decoder, &opts);
    if (ret < 0) {
      LOG(ERROR) << "failed to open video codec:" << avutils::error_msg(ret);
      return video_error::STREAM_INITIALIZATION_ERROR;
    }

    _sink.on_next(encoded_metadata{
        _decoder->name,
        {_decoder_context->extradata,
         _decoder_context->extradata + _decoder_context->extradata_size}});

    return {};
  }

 private:
  void read_loop() {
    while (_active) {
      int ret = av_read_frame(_input_context.get(), &_pkt);
      if (ret == AVERROR_EOF) {
        _sink.on_complete();
        return;
      }

      if (ret < 0) {
        LOG(ERROR) << "error reading packet: " << avutils::error_msg(ret);
        continue;
      }

      auto release = gsl::finally([this]() { av_packet_unref(&_pkt); });
      if (_pkt.stream_index == _stream_idx) {
        LOG(4) << "packet from url " << _url;
        if (_packets == 0) {
          _start_time = _clock.now();
        }
        _packets++;
        int64_t pts = _pkt.pts;
        if (pts < 0) {
          pts = 0;
        }
        int64_t micro_pts = 1000000 * pts * _time_base.num / _time_base.den;
        auto packet_time = _start_time + std::chrono::microseconds(micro_pts);
        encoded_frame frame;
        frame.data = std::string{_pkt.data, _pkt.data + _pkt.size};
        frame.id = {_packets, _packets};
        frame.timestamp = packet_time;
        frame.creation_time = std::chrono::system_clock::now();
        frame.key_frame = static_cast<bool>(_pkt.flags & AV_PKT_FLAG_KEY);
        frames_total.Add({{"url", _url}}).Increment();
        _sink.on_next(frame);
      }
    }
  }

  std::string _url;
  streams::observer<encoded_packet> &_sink;
  std::shared_ptr<AVFormatContext> _input_context;
  AVCodec *_decoder{nullptr};
  AVPacket _pkt{nullptr};
  std::shared_ptr<AVCodecContext> _decoder_context;
  std::thread::id _reader_thread_id;
  std::atomic<bool> _active{true};
  int _stream_idx{-1};
  int64_t _packets{0};
  AVRational _time_base;
  std::chrono::system_clock _clock;
  std::chrono::system_clock::time_point _start_time;
};

streams::publisher<encoded_packet> url_source(const std::string &url,
                                              const std::string &options) {
  return streams::generators<encoded_packet>::async<url_source_impl>(
             [url, options](streams::observer<encoded_packet> &sink) {
               return new url_source_impl(url, options, sink);
             },
             [](url_source_impl *impl) { impl->stop(); })
         >> streams::flatten() >> repeat_metadata();
}
}  // namespace video
}  // namespace satori
