#include "threadutils.h"

#include <cstring>
#include "logging.h"

namespace satori {
namespace video {
namespace threadutils {

constexpr size_t max_name_length = 16;  // including '\0'

void set_current_thread_name(const std::string &original_name) {
  CHECK(!original_name.empty()) << "thread name can't be empty";

  const std::string name = original_name.substr(0, max_name_length - 1);

#if defined(__APPLE__)
  int err = pthread_setname_np(name.c_str());
  CHECK(!err) << "Unable to set thread name to " << name << ": " << strerror(err);
#elif defined(__linux__)
  int err = pthread_setname_np(pthread_self(), name.c_str());
  CHECK(!err) << "Unable to set thread name to " << name << ": " << strerror(err);
#else
  ABORT() << "unknown os";
#endif
}

std::string get_current_thread_name() {
#if defined(__APPLE__) || defined(__linux__)
  char buf[max_name_length];
  int err = pthread_getname_np(pthread_self(), buf, max_name_length);
  CHECK(!err) << "Unable to get thread name: " << strerror(err);
  return std::string{buf};
#else
  ABORT() << "unknown os";
#endif
}

}  // namespace threadutils
}  // namespace video
}  // namespace satori