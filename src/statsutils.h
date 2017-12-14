#pragma once

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/accumulators/statistics/rolling_window.hpp>
#include <cstddef>

namespace satori {
namespace video {
namespace statsutils {

namespace accum = boost::accumulators;

// TODO: might be nicer to have sliding time windows
struct std_dev {
 public:
  explicit std_dev(size_t window) noexcept;

  void emplace(double value) noexcept;

  double value() const noexcept;

 private:
  accum::accumulator_set<double, accum::stats<accum::tag::variance>> _accumulator;
};

}  // namespace statsutils
}  // namespace video
}  // namespace satori