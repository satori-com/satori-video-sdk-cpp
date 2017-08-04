#pragma once

#include <functional>
#include <memory>

#include "sink.h"

namespace rtm {
namespace video {

void initialize_source_library();

template <typename Metadata, typename Frame>
struct source {
 public:
  virtual ~source() = default;
  virtual int init() = 0;
  virtual void start() = 0;

  void subscribe(std::shared_ptr<sink<Metadata, Frame>> sink) {
    _sinks.push_back(std::move(sink));
  }

 protected:
  void foreach_sink(
      std::function<void(const std::shared_ptr<sink<Metadata, Frame>>)>
          sink_fun) {
    for (const auto &s : _sinks) {
      sink_fun(s);
    }
  }

 private:
  std::vector<std::shared_ptr<sink<Metadata, Frame>>> _sinks;
};

}  // namespace video
}  // namespace rtm
