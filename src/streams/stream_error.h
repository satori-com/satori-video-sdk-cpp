#pragma once

#include <system_error>
#include <type_traits>

namespace satori {
namespace video {
namespace streams {

template <typename X>
constexpr bool is_error_condition() {
  using x = typename std::decay<X>::type;
  return std::is_same<std::error_condition, x>::value;
}

enum class stream_error : uint8_t {
  VALUE_WAS_MOVED = 1,
  NOT_INITIALIZED = 2,
  TIMEOUT = 3,
  ASIO_ERROR = 4
};

std::error_condition make_error_condition(stream_error e);

}  // namespace streams
}  // namespace video
}  // namespace satori

namespace std {
template <>
struct is_error_condition_enum<satori::video::streams::stream_error> : std::true_type {};
}  // namespace std
