#include "metrics.h"

#include <boost/timer/timer.hpp>
#include <gperftools/malloc_extension.h>
#include <prometheus/exposer.h>

#include "logging.h"

namespace satori {
namespace video {

namespace {

const auto process_metrics_update_period = boost::posix_time::seconds(1);

auto& process_current_allocated_bytes = prometheus::BuildGauge()
    .Name("process_current_allocated_bytes")
    .Register(metrics_registry())
    .Add({});

auto& process_heap_size = prometheus::BuildGauge()
    .Name("process_heap_size")
    .Register(metrics_registry())
    .Add({});

auto& process_cpu_wall_time_sec = prometheus::BuildCounter()
    .Name("process_cpu_wall_time_sec")
    .Register(metrics_registry())
    .Add({});

auto& process_cpu_user_time_sec = prometheus::BuildCounter()
    .Name("process_cpu_user_time_sec")
    .Register(metrics_registry())
    .Add({});

auto& process_cpu_system_time_sec = prometheus::BuildCounter()
    .Name("process_cpu_system_time_sec")
    .Register(metrics_registry())
    .Add({});


std::shared_ptr<prometheus::Registry> metrics_registry_shared_ptr() {
  static auto registry = std::make_shared<prometheus::Registry>();
  return registry;
}

void report_process_metrics_impl(boost::asio::deadline_timer *timer, boost::timer::cpu_timer *cpu_timer) {
  timer->expires_from_now(process_metrics_update_period);
  timer->async_wait([timer, cpu_timer](const boost::system::error_code ec) {
    if (ec) {
      LOG(ERROR) << ec.message();
      return;
    }

    report_process_metrics_impl(timer, cpu_timer);
  });

  // scrape tcmalloc
  size_t allocated_bytes = 0;
  size_t heap_size = 0;
  CHECK(MallocExtension::instance()->GetNumericProperty("generic.current_allocated_bytes", &allocated_bytes));
  CHECK(MallocExtension::instance()->GetNumericProperty("generic.heap_size", &heap_size));
  process_current_allocated_bytes.Set(allocated_bytes);
  process_heap_size.Set(heap_size);
  
  // scrape cpu timer
  const boost::timer::cpu_times &times = cpu_timer->elapsed();
  process_cpu_system_time_sec.Increment(
      times.system / 1e9 - process_cpu_system_time_sec.Value()
  );
  process_cpu_user_time_sec.Increment(
      times.user / 1e9 - process_cpu_user_time_sec.Value()
  );
  process_cpu_wall_time_sec.Increment(
      times.wall / 1e9 - process_cpu_wall_time_sec.Value()
  );
}

}  // namespace

prometheus::Registry& metrics_registry() { return *metrics_registry_shared_ptr(); }

void expose_metrics(const std::string& bind_address) {
  try {
    auto exposer = new prometheus::Exposer(bind_address);
    exposer->RegisterCollectable(metrics_registry_shared_ptr());
    LOG(INFO) << "Metrics exposed on " << bind_address << "/metrics";
  } catch (const std::exception& e) {
    LOG(ERROR) << "Can't start metrics server on " << bind_address << " : " << e.what();
  }
}

void report_process_metrics(boost::asio::io_service &io) {
  auto timer = new boost::asio::deadline_timer(io);
  auto cpu_timer = new boost::timer::cpu_timer();
  report_process_metrics_impl(timer, cpu_timer);
}

}  // namespace video
}  // namespace satori
