#include <signal.h>

#include "signal_breaker.h"

namespace streams {
namespace {

std::atomic<bool> flag{true};

void sig_handler(int signum) {
  LOG(INFO) << "received signal " << signum << ", breaking the stream";
  flag = false;
}

}  // namespace

namespace impl {
std::atomic<bool> &init_signal_breaker(std::initializer_list<int> signals) {
  static bool initialized{false};
  CHECK(!initialized) << "more than one instance of signal breaker";
  for (int s : signals) {
    signal(s, sig_handler);
  }
  initialized = true;
  return flag;
}
}  // namespace impl
}  // namespace streams