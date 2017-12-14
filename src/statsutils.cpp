#include "statsutils.h"

namespace satori {
namespace video {
namespace statsutils {

std_dev::std_dev(size_t window) noexcept
    : _accumulator{accum::tag::rolling_window::window_size = window} {}

void std_dev::emplace(double value) noexcept { _accumulator(value); }

double std_dev::value() const noexcept {
  auto n = static_cast<double>(accum::count(_accumulator));
  return std::sqrt((accum::variance(_accumulator) * n) / n - 1);
}

}  // namespace statsutils
}  // namespace video
}  // namespace satori