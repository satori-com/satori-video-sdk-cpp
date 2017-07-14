#pragma once

#include <string>

#include "librtmvideo/data.h"

namespace rtm {
namespace video {

struct sink {
 public:
  virtual ~sink() = default;

  virtual void on_metadata(const metadata &m) = 0;

  virtual void on_frame(const encoded_frame &f) = 0;
};

}  // namespace video
}  // namespace rtm