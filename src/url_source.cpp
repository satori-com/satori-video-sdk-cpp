#include "video_streams.h"

#include <gsl/gsl>
#include <thread>

extern "C" {
#include <libavformat/avformat.h>
}

#include "avutils.h"
#include "metrics.h"
#include "threadutils.h"
#include "video_error.h"

namespace satori {
namespace video {

namespace {

auto &frames_total = prometheus::BuildCounter()
                         .Name("url_source_frames_total")
                         .Register(metrics_registry());

auto &created_total = prometheus::BuildCounter()
                          .Name("url_source_created_total")
                          .Register(metrics_registry());

auto &destroyed_total = prometheus::BuildCounter()
                            .Name("url_source_destroyed_total")
                            .Register(metrics_registry());

auto &complete_total = prometheus::BuildCounter()
                           .Name("url_source_complete_total")
                           .Register(metrics_registry());
}  // namespace

class url_source_impl {
 public:
  url_source_impl(const std::string &url, const std::string &options,
                  streams::observer<encoded_packet> &sink)
      : _url{url}, _sink{sink}, _reader_thread_name{"url " + url} {
    avutils::init();
    created_total.Add({{"url", _url}}).Increment();
    std::thread([this, options]() {
      threadutils::set_current_thread_name(_reader_thread_name);

      _reader_thread_id = std::this_thread::get_id();

      auto self_destroyer = gsl::finally([this]() {
        LOG(INFO) << "delete self: " << _url;
        delete this;
      });

      const std::error_condition ec = start(options);
      if (ec) {
        LOG(ERROR) << "unable to start url source " << _url
                   << ", error: " << ec.message();
        _sink.on_error(ec);
        return;
      }

      read_loop();
    })
        .detach();
  }

  ~url_source_impl() {
    LOG(INFO) << "destroying url source: " << _url;
    destroyed_total.Add({{"url", _url}}).Increment();
  }

  void stop() {
    if (_reader_thread_id != std::this_thread::get_id()) {
      LOG(ERROR) << "calling url_source stop from thread {"
                 << threadutils::get_current_thread_name() << ", "
                 << std::this_thread::get_id() << "} but expected {"
                 << _reader_thread_name << ", " << _reader_thread_id << "}";
    }
    LOG(INFO) << "stopping url source";
    _active = false;
  }

  std::error_condition start(const std::string &options) {
    AVDictionary *options_dict{nullptr};
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
      av_init_packet(&_pkt);
      auto release = gsl::finally([this]() { av_packet_unref(&_pkt); });

      int ret = av_read_frame(_input_context.get(), &_pkt);
      if (ret == AVERROR_EOF) {
        LOG(INFO) << "url source is complete: " << _url;
        complete_total.Add({{"url", _url}}).Increment();
        _sink.on_complete();
        return;
      }

      if (ret < 0) {
        LOG(ERROR) << "error reading packet: " << avutils::error_msg(ret);
        continue;
      }

      if (_pkt.stream_index == _stream_idx) {
        LOG(4) << "packet from url " << _url;
        if (_packets == 0) {
          _start_time = _clock.now();
        }
        _packets++;
        // TODO: check how to measure _pkt.pts jitter
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

  const std::string _url;
  streams::observer<encoded_packet> &_sink;
  std::shared_ptr<AVFormatContext> _input_context;
  AVCodec *_decoder{nullptr};
  AVPacket _pkt{nullptr};
  std::shared_ptr<AVCodecContext> _decoder_context;
  std::thread::id _reader_thread_id;
  const std::string _reader_thread_name;
  std::atomic<bool> _active{true};
  int _stream_idx{-1};
  int64_t _packets{0};
  AVRational _time_base{0, 0};
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
