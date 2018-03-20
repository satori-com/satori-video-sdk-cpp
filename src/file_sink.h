#pragma once

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <chrono>

namespace satori {
namespace video {
namespace file_sink {

struct options {
  bool pool_mode{false};
  boost::optional<std::chrono::system_clock::duration> segment_duration;
  boost::filesystem::path path;
};

// TODO: consider supporting json input
struct mkv_options {
  int reserved_index_space{0};
};

}  // namespace file_sink
}  // namespace video
}  // namespace satori