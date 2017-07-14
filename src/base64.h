#pragma once

#include <string>

namespace rtm {
namespace video {

std::string decode64(const std::string &val);
std::string encode64(const std::string &val);

}  // namespace video
}  // namespace rtm
