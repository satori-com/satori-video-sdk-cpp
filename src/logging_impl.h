#pragma once

#define LOGURU_IMPLEMENTATION 1
#define LOGURU_REPLACE_GLOG 1
#include <loguru/loguru.hpp>

#include "logging.h"
#include "satorivideo/base.h"
#include "version.h"

namespace satori {
namespace video {
inline void init_logging(int& argc, char* argv[]) {
  if (RELEASE_MODE) {
    loguru::g_stderr_verbosity = loguru::Verbosity_INFO;
  } else {
    loguru::g_flush_interval_ms = 0;  // unbuffered mode
    loguru::g_stderr_verbosity = loguru::Verbosity_1;
  }
  loguru::init(argc, argv);
  LOG(INFO) << "logging initialized in " << (RELEASE_MODE ? "release" : "debug")
            << " mode";
  log_library_version(loguru::Verbosity_INFO);
}
}  // namespace video
}  // namespace satori
