#pragma once

#include <boost/asio.hpp>
#include <prometheus/counter.h>
#include <prometheus/counter_builder.h>
#include <prometheus/histogram.h>
#include <prometheus/histogram_builder.h>
#include <prometheus/registry.h>

namespace satori {
namespace video {

prometheus::Registry& metrics_registry();

void expose_metrics(const std::string& bind_address);
void report_process_metrics(boost::asio::io_service& io);

}  // namespace video
}  // namespace satori