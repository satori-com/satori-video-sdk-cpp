#include "video_error.h"

#include <string>

namespace satori {
namespace video {

struct video_error_category : std::error_category {
  const char *name() const noexcept override { return "video_error"; }
  std::string message(int ev) const override {
    switch (static_cast<video_error>(ev)) {
      case video_error::STREAM_INITIALIZATION_ERROR:
        return "can't initialize video stream";
      case video_error::FRAME_GENERATION_ERROR:
        return "can't generate video frame";
      case video_error::ASIO_ERROR:
        return "asio error";
      case video_error::END_OF_STREAM_ERROR:
        return "end of video stream";
      case video_error::FRAME_NOT_READY_ERROR:
        return "frame not ready";
    }
  }
};

std::error_condition make_error_condition(video_error e) {
  static video_error_category category;
  return {static_cast<int>(e), category};
}

}  // namespace video
}  // namespace satori