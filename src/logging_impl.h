#pragma once

#define LOGURU_IMPLEMENTATION 1
#define LOGURU_REPLACE_GLOG 1
#include <loguru/loguru.hpp>

#include "librtmvideo/base.h"
#include "logging.h"

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
}