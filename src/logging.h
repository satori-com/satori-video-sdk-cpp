#pragma once

#define LOGURU_REPLACE_GLOG 1
#include <loguru/loguru.hpp>

#define ABORT ABORT_S

// Fixes clang-tidy complaints.
#undef CHECK
#define CHECK(cond) CHECK_S(cond)

// Fixes clang-tidy complaints.
#undef CHECK_WITH_INFO_S
#define CHECK_WITH_INFO_S(cond, info)          \
  LOGURU_PREDICT_TRUE(static_cast<bool>(cond)) \
  ? (void)0                                    \
  : loguru::Voidify()                          \
          & loguru::AbortLogger("CHECK FAILED:  " info "  ", __FILE__, __LINE__)
