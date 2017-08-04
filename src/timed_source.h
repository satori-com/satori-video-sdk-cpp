#pragma once

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/optional.hpp>
#include <chrono>

#include "source.h"

namespace rtm {
namespace video {

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
