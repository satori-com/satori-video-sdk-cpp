#include <thread>
#include "avutils.h"
#include "threadutils.h"
#include "video_error.h"
#include "video_streams.h"

extern "C" {
#include <libavformat/avformat.h>
}

namespace satori {
namespace video {

struct url_source_impl {
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
      return video_error::StreamInitializationError;
    }
    _input_context = avutils::open_input_format_context(_url, nullptr, options_dict);
    if (!_input_context) {
      return video_error::StreamInitializationError;
    }

    _stream_idx = avutils::find_best_video_stream(_input_context.get(), &_decoder);
    if (_stream_idx < 0) {
      return video_error::StreamInitializationError;
    }
    AVStream *stream = _input_context->streams[_stream_idx];

    av_read_play(_input_context.get());

    _decoder_context = avutils::decoder_context(_decoder);
    if (!_decoder_context) {
      return video_error::StreamInitializationError;
    }

    int ret = avcodec_parameters_to_context(_decoder_context.get(), stream->codecpar);
    if (ret < 0) {
      LOG(ERROR) << "failed to copy codec parameters to codec context:"
                 << avutils::error_msg(ret);
      return video_error::StreamInitializationError;
    }

    AVDictionary *opts = nullptr;
    ret = avcodec_open2(_decoder_context.get(), _decoder, &opts);
    if (ret < 0) {
      LOG(ERROR) << "failed to open video codec:" << avutils::error_msg(ret);
      return video_error::StreamInitializationError;
    }

    _sink.on_next(encoded_metadata{
        _decoder->name,
        {_decoder_context->extradata,
         _decoder_context->extradata + _decoder_context->extradata_size}});

    return {};
  }

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
        _next_id++;
        encoded_frame frame{std::string{_pkt.data, _pkt.data + _pkt.size},
                            frame_id{_next_id, _next_id}};
        frame.key_frame = static_cast<bool>(_pkt.flags & AV_PKT_FLAG_KEY);
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
  int64_t _next_id{0};
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
