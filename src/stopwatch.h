#pragma once

#include <chrono>

namespace rtm {
namespace video {

template <class Clock = std::chrono::system_clock>
class stopwatch {
 public:
  stopwatch() : _start(Clock::now()) {}

  uint64_t millis() {
    auto d = Clock::now() - _start;
    return std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
  }

 private:
  typename Clock::time_point _start;
};

}  // namespace video
}  // namespace rtm