#pragma once

#include <string>

namespace satori {
namespace video {
namespace threadutils {

void set_current_thread_name(const std::string &original_name);

std::string get_current_thread_name();

}  // namespace threadutils
}  // namespace video
}  // namespace satori