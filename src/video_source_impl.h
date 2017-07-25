#pragma once

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <chrono>
#include <functional>

namespace rtm {
namespace video {

using metadata_subscriber =
    std::function<void(const char *, size_t, const uint8_t *)>;
using frames_subscriber = std::function<void(size_t, const uint8_t *)>;

void initialize_sources_library();
void print_av_error(const char *msg, int code);

struct source {
 public:
  virtual ~source() = default;
  virtual int init() = 0;
  virtual void start() = 0;
  void subscribe(metadata_subscriber &&metadata_subscriber,
                 frames_subscriber &&frames_subscriber);

 protected:
  std::vector<metadata_subscriber> _metadata_subscribers;
  std::vector<frames_subscriber> _frames_subscribers;
};

struct timed_source : public source {
 protected:
  void start(const std::string &codec_name, size_t codec_data_len,
             const uint8_t *codec_data,
             std::chrono::milliseconds frames_interval,
             std::chrono::milliseconds metadata_interval);
  void stop_timers();
  virtual int next_packet(uint8_t **output) = 0;  // TODO: better method API?

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
  size_t _codec_data_len;
  const uint8_t *_codec_data{nullptr};
};
}  // namespace video
}  // namespace rtm