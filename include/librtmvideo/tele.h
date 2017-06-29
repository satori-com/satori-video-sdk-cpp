// Telemetry reporting
#pragma once

#include <stdint.h>
#include "base.h"

namespace tele {
constexpr char channel[] = "tele";

EXPORT struct counter;

EXPORT counter *counter_new(const char *group, const char *name);
EXPORT void counter_inc(counter *counter, uint64_t delta = 1);
}  // namespace tele
