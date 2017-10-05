#pragma once

#include <string>

namespace satori {
namespace video {

std::string decode64(const std::string &val);
std::string encode64(const std::string &val);

}  // namespace video
}  // namespace satori
