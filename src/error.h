// Error code for video sdk
#pragma once

#include <system_error>

namespace rtm {
namespace video {

// Video processing error codes.
// Error codes should only be as granular as their processing requires.
// All specifics should be logged at the location the error has happened.
enum class video_error : uint8_t {
  // 0 = success
  StreamInitializationError = 1,
  FrameGenerationError = 2,
  AsioError = 3,
  EndOfStreamError = 4,
  FrameNotReadyError = 5,
};

std::error_condition make_error_condition(video_error e);

}  // namespace video
}  // namespace rtm

namespace std {
template <>
struct is_error_condition_enum<rtm::video::video_error> : std::true_type {};
}  // namespace std
