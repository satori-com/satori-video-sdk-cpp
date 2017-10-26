#pragma once

#include <prometheus/counter.h>
#include <prometheus/counter_builder.h>
#include <prometheus/histogram.h>
#include <prometheus/histogram_builder.h>
#include <prometheus/registry.h>

namespace satori {
namespace video {

prometheus::Registry& metrics_registry();

void expose_metrics(const std::string& bind_address);

}  // namespace video
}  // namespace satori