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
  static bool initialized = false;
  CHECK(!initialized);
  initialized = true;

  if (RELEASE_MODE) {
    loguru::g_stderr_verbosity = loguru::Verbosity_INFO;
  } else {
    loguru::g_flush_interval_ms = 0;  // unbuffered mode
    loguru::g_stderr_verbosity = loguru::Verbosity_1;
  }
  loguru::init(argc, argv);
  LOG(INFO) << "logging initialized in " << (RELEASE_MODE ? "release" : "debug")
            << " mode";
  VLOG(loguru::Verbosity_INFO) << library_version();

  loguru::set_fatal_handler([](const loguru::Message& message) {
    std::cerr << "*** This program encountered an unrecoverable error and is "
                 "terminating, bye...\n"
              << "*** " << library_version();
  });

  static auto old_terminate_handler = std::get_terminate();
  std::set_terminate([](){
    auto e = std::current_exception();
    if (e) {
      LOG(ERROR) << "unexpected exception";
    }

    old_terminate_handler();
  });
}

}  // namespace video
}  // namespace satori
