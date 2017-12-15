#pragma once

#include <functional>
#include <initializer_list>

namespace satori {
namespace video {
namespace signal {

using signal_handler_fn = std::function<void(int signal)>;

void register_handler(std::initializer_list<int> signals,
                      signal_handler_fn const &signal_handler);

}  // namespace signal
}  // namespace video
}  // namespace satori