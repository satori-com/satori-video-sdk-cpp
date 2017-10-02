#include "error.h"

#include <string>

namespace rtm {
namespace video {

struct video_error_category : std::error_category {
  const char *name() const noexcept override { return "video_error"; }
  std::string message(int ev) const override {
    switch (static_cast<video_error>(ev)) {
      case video_error::StreamInitializationError:
        return "can't initialize video stream";
      case video_error::FrameGenerationError:
        return "can't generate video frame";
      case video_error::AsioError:
        return "asio error";
      case video_error::EndOfStreamError:
        return "end of video stream";
      case video_error::FrameNotReadyError:
        return "frame not ready";
      case video_error::ValueWasMoved:
        return "value was moved'";
    }
  }
};

std::error_condition make_error_condition(video_error e) {
  static video_error_category category;
  return std::error_condition(static_cast<int>(e), category);
}

}  // namespace video
}  // namespace rtm