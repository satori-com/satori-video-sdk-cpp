// Stream operator that cancels the stream when signal arrives.
// Only one instance of signal_breaker can exist in the program.
#pragma once

#include "streams.h"

namespace streams {

namespace impl {
std::atomic<bool> &init_signal_breaker(std::initializer_list<int> signals);
}

template <typename T>
op<T, T> signal_breaker(std::initializer_list<int> signals) {
  return [signals](publisher<T> &&src) {
    std::atomic<bool> &flag = impl::init_signal_breaker(signals);
    return std::move(src) >> take_while([&flag](const T & /*t*/) { return flag.load(); });
  };
};

}  // namespace streams