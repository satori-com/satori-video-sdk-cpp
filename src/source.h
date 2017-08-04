#pragma once

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/optional.hpp>
#include <chrono>
#include <functional>

#include "sink.h"

namespace rtm {
namespace video {

void initialize_source_library();
void print_av_error(const char *msg, int code);

template <typename Metadata, typename Frame>
struct source {
 public:
  virtual ~source() = default;
  virtual int init() = 0;
  virtual void start() = 0;

  void subscribe(std::shared_ptr<sink<Metadata, Frame>> sink) {
    _sinks.push_back(std::move(sink));
  }

 protected:
  std::vector<std::shared_ptr<sink<Metadata, Frame>>> _sinks;
};

struct timed_source : public source<metadata, encoded_frame> {
 protected:
  void start(const std::string &codec_name, const std::string &codec_data,
             std::chrono::milliseconds frames_interval,
             std::chrono::milliseconds metadata_interval);
  void stop_timers();
  virtual boost::optional<std::string> next_packet() = 0;
  void codec_init(const std::string &codec_name, const std::string &codec_data,
                  std::chrono::milliseconds metadata_interval);

 private:
  void metadata_tick();
  void frames_tick();

 private:
  boost::asio::io_service _io_service;
  std::chrono::milliseconds _frames_interval;
  boost::asio::deadline_timer _frames_timer{_io_service};
  std::chrono::milliseconds _metadata_interval;
  boost::asio::deadline_timer _metadata_timer{_io_service};
  std::string _codec_name;
  std::string _codec_data;
};
}  // namespace video
}  // namespace rtm
