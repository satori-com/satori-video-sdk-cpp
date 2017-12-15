#pragma once

#include <prometheus/counter.h>
#include <prometheus/counter_builder.h>
#include <prometheus/histogram.h>
#include <prometheus/histogram_builder.h>
#include <prometheus/registry.h>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/options_description.hpp>
#include "rtm_client.h"

namespace satori {
namespace video {

prometheus::Registry& metrics_registry();

boost::program_options::options_description metrics_options();

void init_metrics(const boost::program_options::variables_map& cmd_args,
                  boost::asio::io_service& io_service);

void expose_metrics(rtm::publisher* publisher);

void stop_metrics();

}  // namespace video
}  // namespace satori