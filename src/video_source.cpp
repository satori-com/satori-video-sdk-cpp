#include <iostream>
#include <boost/bind.hpp>
#include "librtmvideo/video_source.h"
#include "video_source_impl.h"
#include "video_source_camera.h"
#include "video_source_file.h"

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
}

namespace rtm {
namespace video {

void initialize_sources_library() {
  static bool is_initialized = false;

  if(!is_initialized) {
    avdevice_register_all();
    avcodec_register_all();
    av_register_all();
    is_initialized = true;
  }
}

void print_av_error(const char *msg, int code) {
  char av_error[AV_ERROR_MAX_STRING_SIZE];
  std::cerr << msg
            << ", code: " << code
            << ", error: \"" << av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, code) << "\"\n";
}

void source::subscribe(metadata_subscriber &&metadata_subscriber, frames_subscriber &&frames_subscriber) {
  _metadata_subscribers.push_back(std::move(metadata_subscriber));
  _frames_subscribers.push_back(std::move(frames_subscriber));
}

void timed_source::start(const std::string &codec_name,
                         size_t codec_data_len,
                         const uint8_t *codec_data,
                         std::chrono::milliseconds frames_interval,
                         std::chrono::milliseconds metadata_interval) {
  _frames_interval = frames_interval;
  _metadata_interval = metadata_interval;
  _codec_name = codec_name;
  _codec_data_len = codec_data_len;
  _codec_data = codec_data;

  metadata_tick();

  _frames_timer.expires_from_now(boost::posix_time::milliseconds(_frames_interval.count()));
  _frames_timer.async_wait(boost::bind(&timed_source::frames_tick, this));

  _io_service.run();
}

void timed_source::stop_timers() {
  _io_service.stop();
}

void timed_source::metadata_tick() {
  for(metadata_subscriber &s : _metadata_subscribers) {
    // TODO: not sure if we need to copy memory
    s(_codec_name.c_str(), _codec_data_len, _codec_data);
  }

  _metadata_timer.expires_at(
      _metadata_timer.expires_at() + boost::posix_time::milliseconds(_metadata_interval.count()));
  _metadata_timer.async_wait(boost::bind(&timed_source::metadata_tick, this));
}

void timed_source::frames_tick() {
  uint8_t *data;
  int data_len = next_packet(&data);

  if(data_len > 0) {
    for(frames_subscriber &s : _frames_subscribers) {
      s(data_len, data);
    }
    delete[] data;
  }

  _frames_timer.expires_at(
      _frames_timer.expires_at() + boost::posix_time::milliseconds(_frames_interval.count()));
  _frames_timer.async_wait(boost::bind(&timed_source::frames_tick, this));
}

} // namespace video
} // namespace rtm


// C-style API

struct video_source {
  std::unique_ptr<rtm::video::source> source;
};

void video_source_init_library() {
  rtm::video::initialize_sources_library();
}

video_source *video_source_camera_new(const char *dimensions) {
  video_source_init_library();

  std::unique_ptr<video_source> vs(new video_source());
  vs->source = std::make_unique<rtm::video::camera_source>(dimensions);
  std::cout << "*** Initializing camera video source...\n";
  int err = vs->source->init();
  if(err) {
    std::cerr << "*** Error initializing camera video source, error code " << err << "\n";
    return nullptr;
  }
  std::cout << "*** Camera video source was initialized\n";

  return vs.release();
}

video_source *video_source_file_new(const char *filename, int is_replayed) {
  video_source_init_library();

  std::unique_ptr<video_source> vs(new video_source());
  vs->source = std::make_unique<rtm::video::file_source>(filename, static_cast<bool>(is_replayed));
  std::cout << "*** Initializing file video source...\n";
  int err = vs->source->init();
  if(err) {
    std::cerr << "*** Error initializing file video source, error code " << err << "\n";
    return nullptr;
  }
  std::cout << "*** File video source was initialized\n";

  return vs.release();
}

void video_source_delete(video_source *video_source) {
  delete video_source;
}

void video_source_subscribe(video_source *video_source,
                            metadata_handler metadata_handler,
                            frame_handler frame_handler) {
  video_source->source->subscribe(metadata_handler, frame_handler);
}

void video_source_start(video_source *video_source) {
  video_source->source->start();
}
