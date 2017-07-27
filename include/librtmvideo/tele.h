// Telemetry reporting
#pragma once

#include <stdint.h>
#include "base.h"

namespace tele {

// RTM channel for telemetry data. Values of all telemetry counters are
// published every second. Messages look like: {"id": "dummy-host-name",
// "counters": {"key1": 3, "key2": 2, "key3": 10}}
constexpr char channel[] = "tele";

// Counter struct is opaque to users.
EXPORT struct counter;

// Returns a pointer to newly created counter.
EXPORT counter *counter_new(const char *group, const char *name);

// Increments counter value.
EXPORT void counter_inc(counter *counter, uint64_t delta = 1);

EXPORT struct gauge;
EXPORT gauge *gauge_new(const char *group, const char *name);
EXPORT void gauge_set(gauge *gauge, int64_t value);

EXPORT struct distribution;
EXPORT distribution *distribution_new(const char *group, const char *name);
EXPORT void distribution_add(distribution *distribution, int64_t value);

}  // namespace tele
