#pragma once

#include "librtmvideo/base.h"

#define LOGURU_IMPLEMENTATION 1
#define LOGURU_WITH_STREAMS 1
#include <loguru/loguru.hpp>

inline void init_logging(int &argc, char* argv[]) {
  if (RELEASE_MODE) {
    loguru::g_stderr_verbosity = loguru::Verbosity_INFO;
  } else {
    loguru::g_flush_interval_ms = 0;  // unbuffered mode
    loguru::g_stderr_verbosity = loguru::Verbosity_1;
  }
  loguru::init(argc, argv);
  LOG_S(INFO) << "logging initialized in " << (RELEASE_MODE ? "release" : "debug")
              << " mode";
}