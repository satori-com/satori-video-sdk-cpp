#pragma once

#include <string>

#include "librtmvideo/data.h"

namespace rtm {
namespace video {

template <typename Metadata, typename Frame>
struct sink {
 public:
  virtual ~sink() = default;

  virtual void on_metadata(Metadata &&m) = 0;

  virtual void on_frame(Frame &&f) = 0;

  virtual bool empty() { return true; }
};

}  // namespace video
}  // namespace rtm
