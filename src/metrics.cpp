#include "metrics.h"

#include <prometheus/exposer.h>

#include "logging.h"

namespace satori {
namespace video {

namespace {
std::shared_ptr<prometheus::Registry> metrics_registry_shared_ptr() {
  static auto registry = std::make_shared<prometheus::Registry>();
  return registry;
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

}  // namespace video
}  // namespace satori