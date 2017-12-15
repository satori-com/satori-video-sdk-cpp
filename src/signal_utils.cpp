#include "signal_utils.h"

#include <csignal>
#include <cstring>
#include <map>
#include <vector>

#include "logging.h"

namespace satori {
namespace video {
namespace signal {

namespace {

std::map<int, std::vector<signal_handler_fn>> handlers_map;

void on_signal(int signal) {
  LOG(INFO) << "caught signal " << strsignal(signal);

  auto it = handlers_map.find(signal);
  if (it != handlers_map.end()) {
    std::vector<signal_handler_fn> &signal_handlers = it->second;

    for (auto &h : signal_handlers) {
      h(signal);
    }
  }
}

}  // namespace

void register_handler(std::initializer_list<int> signals,
                      signal_handler_fn const &signal_handler) {
  for (int s : signals) {
    auto it = handlers_map.find(s);
    if (it != handlers_map.end()) {
      std::vector<signal_handler_fn> &signal_handlers = it->second;
      signal_handlers.emplace_back(signal_handler);
    } else {
      std::vector<signal_handler_fn> v{signal_handler};
      handlers_map.emplace(s, std::move(v));
      std::signal(s, on_signal);
    }
  }
}

}  // namespace signal
}  // namespace video
}  // namespace satori