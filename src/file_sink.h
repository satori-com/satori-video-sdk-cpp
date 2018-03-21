#pragma once

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <chrono>

#include "data.h"

namespace satori {
namespace video {
namespace file_sink {

// TODO: add --segment-frames parameter
struct options {
  // TODO: add input channel and resolution
  bool pool_mode{false};
  boost::optional<std::chrono::system_clock::duration> segment_duration;
  boost::filesystem::path path;
  std::string channel;
  image_size resolution;
};

// TODO: consider supporting json input
struct mkv_options {
  int reserved_index_space{0};
};

}  // namespace file_sink
}  // namespace video
}  // namespace satori