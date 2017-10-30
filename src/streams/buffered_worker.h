#pragma once

#include <boost/variant.hpp>
#include <functional>
#include <thread>

#include "../metrics.h"

#include "channel.h"
#include "streams.h"

namespace satori {
namespace video {
namespace streams {

namespace impl {

static prometheus::Family<prometheus::Counter> &dropped_items_family =
    prometheus::BuildCounter().Name("dropped_items").Register(metrics_registry());
static prometheus::Family<prometheus::Counter> &delivered_items_family =
    prometheus::BuildCounter().Name("delivered_items").Register(metrics_registry());
static prometheus::Family<prometheus::Gauge> &buffer_size_family =
    prometheus::BuildGauge().Name("buffer_size").Register(metrics_registry());

struct buffered_worker_op {
  buffered_worker_op(const std::string &name, size_t buffer_size)
      : _name(name), _size(buffer_size) {}

  template <typename T>
  struct instance : streams::subscriber<T>, streams::subscription,
                    boost::static_visitor<void> {
    using value_t = T;

    struct subscribe {};
    struct complete {};
    struct error {
      std::error_condition ec;
    };
    struct next {
      T t;
    };
    using msg = boost::variant<subscribe, next, error, complete>;

    static streams::publisher<T> apply(streams::publisher<T> &&src,
                                       buffered_worker_op &&op) {
      return streams::publisher<T>(
          new streams::impl::op_publisher<T, T, buffered_worker_op>(std::move(src),
                                                                    std::move(op)));
    }

    instance(buffered_worker_op &&op, streams::subscriber<T> &sink)
        : _name(op._name),
          _sink(sink),
          _channel(op._size),
          _dropped_items(dropped_items_family.Add({{"buffer_name", _name}})),
          _delivered_items(delivered_items_family.Add({{"buffer_name", _name}})),
          _buffer_size(buffer_size_family.Add({{"buffer_name", _name}})) {
      _worker_thread = std::make_unique<std::thread>(&instance::worker_thread_loop, this);
    }

    ~instance() { LOG(5) << "buffered_worker_op(" << this << ")::~buffered_worker_op"; }

    void on_next(T &&t) override {
      LOG(5) << "buffered_worker_op(" << this << ")::on_next";
      if (!_channel.try_send(next{std::move(t)})) {
        _dropped_items.Increment();
      }
      _buffer_size.Set(_channel.size());
      _source_sub->request(1);
    };

    void on_error(std::error_condition ec) override {
      LOG(5) << "buffered_worker_op(" << this << ")::on_error";
      _channel.send(error{ec});
      _source_sub = nullptr;
    };

    void on_complete() override {
      LOG(5) << "buffered_worker_op(" << this << ")::on_complete";
      _channel.send(complete{});
      _source_sub = nullptr;
    };

    void on_subscribe(subscription &s) override {
      _source_sub = &s;
      _channel.send(subscribe{});
      _source_sub->request(1);
    };

    void request(int n) override { _outstanding += n; }

    void cancel() override {
      LOG(5) << "buffered_worker_op(" << this << ")::cancel";
      if (_source_sub) {
        _source_sub->cancel();
        _source_sub = nullptr;
      }
      _is_active = false;
    }

    void operator()(const subscribe &) {
      LOG(6) << "buffered_worker_op(" << this << ")::worker_thread_loop on_subscribe";
      _sink.on_subscribe(*this);
    }

    void operator()(next &n) {
      CHECK_GT(_outstanding, 0) << "too many messages in " << _name;
      LOG(6) << "buffered_worker_op(" << this << ")::worker_thread_loop >on_next";
      _sink.on_next(std::move(n.t));
      LOG(6) << "buffered_worker_op(" << this << ")::worker_thread_loop <on_next";
      _outstanding--;
      _delivered_items.Increment();
    }

    void operator()(const complete &) {
      LOG(6) << "buffered_worker_op(" << this << ")::worker_thread_loop complete";
      _sink.on_complete();
      // break the loop
      _is_active = false;
    }

    void operator()(const error &e) {
      LOG(6) << "buffered_worker_op(" << this << ")::worker_thread_loop error";
      _sink.on_error(e.ec);
      // break the loop
      _is_active = false;
    }

   private:
    void worker_thread_loop() noexcept {
      while (_is_active) {
        LOG(6) << "buffered_worker_op(" << this << ")::worker_thread_loop waiting";
        msg m = _channel.recv();
        LOG(6) << "buffered_worker_op(" << this
               << ")::worker_thread_loop got msg _is_active=" << _is_active;
        if (!_is_active) {
          // we are no longer active after waiting for an item.
          break;
        }

        boost::apply_visitor(*this, m);
      }
      LOG(5) << "buffered_worker_op(" << this << ")::worker_thread_loop exit";
      _worker_thread->detach();
      delete this;
    }

    const std::string _name;
    std::atomic<bool> _is_active{true};
    streams::subscriber<T> &_sink;
    channel<msg> _channel;
    std::unique_ptr<std::thread> _worker_thread;

    prometheus::Counter &_dropped_items;
    prometheus::Counter &_delivered_items;
    prometheus::Gauge &_buffer_size;

    streams::subscription *_source_sub;
    std::atomic<long> _outstanding{0};
  };

  const std::string _name;
  const size_t _size;
};

}  // namespace impl

inline auto buffered_worker(const std::string &name, size_t buffer_size) {
  return impl::buffered_worker_op(name, buffer_size);
}

}  // namespace streams
}  // namespace video
}  // namespace satori
