#pragma once

#include <initializer_list>
#include "../signal_utils.h"
#include "streams.h"

namespace satori {
namespace video {
namespace streams {
namespace impl {

struct signal_breaker_op {
  explicit signal_breaker_op(std::initializer_list<int> signals) : _signals(signals) {
    static int number_of_instances{0};
    CHECK_EQ(number_of_instances, 0) << "only one instance is allowed";
    number_of_instances++;
  }

  template <typename T>
  struct instance : subscriber<T>, subscription {
    instance(signal_breaker_op &&op, subscriber<T> &sink) : _sink(sink) {
      signal::register_handler(op._signals, [this](int /*signal*/) {
        LOG(INFO) << " breaking the stream";
        if (_source_sub != nullptr) {
          LOG(INFO) << "cancelling upstream subscription";
          _source_sub->cancel();
        }
        LOG(INFO) << "sending complete signal to downstream";
        _sink.on_complete();
        delete this;
      });
    }

    static publisher<T> apply(publisher<T> &&source, signal_breaker_op &&op) {
      return publisher<T>(
          new impl::op_publisher<T, T, signal_breaker_op>(std::move(source), op));
    }

    void on_next(T &&t) override { _sink.on_next(std::move(t)); };

    void on_error(std::error_condition ec) override {
      LOG(ERROR) << "stream_breaker_op(" << this << ") on_error";
      _sink.on_error(ec);
      delete this;
    };

    void on_complete() override {
      LOG(5) << "stream_breaker_op(" << this << ") on_complete";
      _sink.on_complete();
      delete this;
    };

    void on_subscribe(subscription &s) override {
      LOG(INFO) << "stream_breaker_op(" << this << ") on_subscribe";
      _source_sub = &s;
      _sink.on_subscribe(*this);
    };

    void request(int n) override {
      LOG(5) << "stream_breaker_op(" << this << ") request " << n;
      _source_sub->request(n);
    }

    void cancel() override {
      LOG(INFO) << "stream_breaker_op(" << this << ") cancel";
      _source_sub->cancel();
      delete this;
    }

    subscriber<T> &_sink;
    subscription *_source_sub{nullptr};
  };

 private:
  const std::initializer_list<int> _signals;
};

}  // namespace impl

// Stream operator that cancels the stream when signal arrives.
// Only one instance of signal_breaker can exist in the program.
inline auto signal_breaker(std::initializer_list<int> signals) {
  return impl::signal_breaker_op(signals);
}

}  // namespace streams
}  // namespace video
}  // namespace satori
