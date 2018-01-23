#pragma once

#include <string>

#include "streams/error_or.h"

namespace satori {
namespace video {
namespace base64 {

constexpr double overhead = 4. / 3.;

streams::error_or<std::string> decode(const std::string &val);
std::string encode(const std::string &val);

}  // namespace base64
}  // namespace video
}  // namespace satori
