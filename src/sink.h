#pragma once

#include <string>

#include "librtmvideo/data.h"

namespace rtm {
namespace video {

template <typename Metadata, typename Frame>
struct sink {
 public:
  virtual ~sink() = default;

  virtual void on_metadata(const Metadata &m) = 0;

  virtual void on_frame(const Frame &f) = 0;

  virtual bool empty() = 0;
};

}  // namespace video
}  // namespace rtm
