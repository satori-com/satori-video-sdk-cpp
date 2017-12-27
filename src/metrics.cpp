#include "metrics.h"

#include <prometheus/exposer.h>
#include <prometheus/text_serializer.h>
#include <boost/timer/timer.hpp>
#include <chrono>
#include <json.hpp>

#ifdef HAS_GPERFTOOLS
#include <gperftools/malloc_extension.h>
#endif

#include "logging.h"

namespace po = boost::program_options;

namespace satori {
namespace video {

namespace {

const auto process_metrics_update_period = boost::posix_time::seconds(1);
const auto metrics_push_period = boost::posix_time::seconds(10);

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

auto& process_start_time = prometheus::BuildGauge()
                               .Name("process_start_time")
                               .Register(metrics_registry())
                               .Add({});

#ifdef HAS_GPERFTOOLS
void report_tcmalloc_metrics() {
  MallocExtension* extension = MallocExtension::instance();
  if (extension == nullptr) {
    LOG(ERROR) << "null malloc extension";
    return;
  }

  {
    size_t allocated_bytes = 0;
    bool success = extension->GetNumericProperty("generic.current_allocated_bytes",
                                                 &allocated_bytes);
    if (success) {
      process_current_allocated_bytes.Set(allocated_bytes);
    } else {
      LOG(ERROR) << "can't get generic.current_allocated_bytes property";
    }
  }

  {
    size_t heap_size = 0;
    bool success = extension->GetNumericProperty("generic.heap_size", &heap_size);
    if (success) {
      process_heap_size.Set(heap_size);
    } else {
      LOG(ERROR) << "can't get generic.heap_size property";
    }
  }
}
#else
void report_tcmalloc_metrics() {}
#endif

void report_process_metrics() {
  report_tcmalloc_metrics();
  static boost::timer::cpu_timer cpu_timer;

  // scrape cpu timer
  const boost::timer::cpu_times& times = cpu_timer.elapsed();
  process_cpu_system_time_sec.Increment(times.system / 1e9
                                        - process_cpu_system_time_sec.Value());
  process_cpu_user_time_sec.Increment(times.user / 1e9
                                      - process_cpu_user_time_sec.Value());
  process_cpu_wall_time_sec.Increment(times.wall / 1e9
                                      - process_cpu_wall_time_sec.Value());
}

class metrics {
 public:
  void init(metrics_config&& config, boost::asio::io_service& io_service) {
    _config = std::move(config);
    _io_service = &io_service;
    start_updating_process_metrics();
  }

  void expose_metrics(rtm::publisher* publisher) {
    if (!_config.bind_address.empty()) {
      expose_http_metrics();
    }

    if (!_config.push_channel.empty()) {
      CHECK(_io_service);
      CHECK(publisher) << "rtm publisher not provided";
      _publisher = publisher;
      _push_timer = new boost::asio::deadline_timer(*_io_service);
      push_metrics();
    }
  }

  void expose_http_metrics() const {
    try {
      auto exposer = new prometheus::Exposer(_config.bind_address);
      exposer->RegisterCollectable(_registry);
      LOG(INFO) << "Metrics exposed on " << _config.bind_address << "/metrics";
    } catch (const std::exception& e) {
      LOG(ERROR) << "Can't start metrics server on " << _config.bind_address << " : "
                 << e.what();
    }
  }

  prometheus::Registry& registry() { return *_registry; }

  void stop() {
    if (_push_timer != nullptr) {
      _push_timer->cancel();
    }
    if (_update_process_metrics_timer != nullptr) {
      _update_process_metrics_timer->cancel();
    }
  }

 private:
  void push_metrics() {
    _push_timer->expires_from_now(metrics_push_period);
    _push_timer->async_wait([this](const boost::system::error_code ec) {
      if (ec.value() != 0) {
        LOG(ERROR) << "timer error: " << ec << " " << ec.message();
        delete _push_timer;
        return;
      }

      push_metrics();
    });

    prometheus::TextSerializer serializer;
    std::string data = serializer.Serialize(metrics_registry().Collect());
    LOG(1) << "pushing metrics " << data.size() << " bytes";

    nlohmann::json msg = nlohmann::json::object();
    msg["content-type"] = "text/plain";
    msg["metrics"] = data;

    if (!_config.push_job.empty()) {
      msg["job"] = _config.push_job;
    }
    if (!_config.push_instance.empty()) {
      msg["instance"] = _config.push_instance;
    }

    _publisher->publish(_config.push_channel, std::move(msg));
  }

  void start_updating_process_metrics() {
    auto time_since_epoch = std::chrono::system_clock::now().time_since_epoch();
    process_start_time.Set(
        std::chrono::duration_cast<std::chrono::seconds>(time_since_epoch).count());
    _update_process_metrics_timer = new boost::asio::deadline_timer(*_io_service);
    update_process_metrics();
  }

  void update_process_metrics() {
    _update_process_metrics_timer->expires_from_now(process_metrics_update_period);
    _update_process_metrics_timer->async_wait([this](const boost::system::error_code ec) {
      if (ec.value() != 0) {
        LOG(ERROR) << "timer error: " << ec << " " << ec.message();
        delete _update_process_metrics_timer;
        return;
      }

      update_process_metrics();
    });

    report_process_metrics();
  }

  std::shared_ptr<prometheus::Registry> _registry{
      std::make_shared<prometheus::Registry>()};

  metrics_config _config;
  boost::asio::io_service* _io_service{nullptr};
  rtm::publisher* _publisher{nullptr};

  boost::asio::deadline_timer* _push_timer{nullptr};
  boost::asio::deadline_timer* _update_process_metrics_timer{nullptr};
};

metrics& global_metrics() {
  static metrics m;
  return m;
}

}  // namespace

prometheus::Registry& metrics_registry() { return global_metrics().registry(); }

po::options_description metrics_options() {
  po::options_description options("Monitoring options");
  options.add_options()("metrics-bind-address",
                        po::value<std::string>()->default_value(""),
                        "address:port for metrics server.");
  options.add_options()("metrics-push-channel",
                        po::value<std::string>()->default_value(""),
                        "rtm channel to push metrics to.");
  options.add_options()("metrics-push-job", po::value<std::string>()->default_value(""),
                        "job value to report while pushing metrics.");
  options.add_options()("metrics-push-instance",
                        po::value<std::string>()->default_value(""),
                        "instance value to report while pushing metrics.");

  return options;
}

metrics_config::metrics_config(const boost::program_options::variables_map& vm)
    : bind_address(vm["metrics-bind-address"].as<std::string>()),
      push_channel(vm["metrics-push-channel"].as<std::string>()),
      push_job(vm["metrics-push-job"].as<std::string>()),
      push_instance(vm["metrics-push-instance"].as<std::string>()) {}

void init_metrics(metrics_config&& config, boost::asio::io_service& io_service) {
  global_metrics().init(std::move(config), io_service);
}

void expose_metrics(rtm::publisher* publisher) {
  global_metrics().expose_metrics(publisher);
}

void stop_metrics() { global_metrics().stop(); }

}  // namespace video
}  // namespace satori
