#pragma once

#include <boost/variant.hpp>
#include <cassert>
#include <functional>
#include <thread>

#include "channel.h"
#include "librtmvideo/tele.h"
#include "streams.h"

namespace rtm {
namespace video {

// streams operator.
struct buffered_worker_op {
  buffered_worker_op(const std::string &name, size_t buffer_size)
      : _name(name), _size(buffer_size) {}

  template <typename T>
  struct instance : streams::subscriber<T>, streams::subscription {
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
        : _sink(sink), _channel(op._size) {
      _worker_thread = std::make_unique<std::thread>(&instance::worker_thread_loop, this);
      _dropped_items = tele::counter_new(op._name.c_str(), "dropped");
      _delivered_items = tele::counter_new(op._name.c_str(), "delivered");
      _buffer_size = tele::gauge_new(op._name.c_str(), "size");
      LOG_S(5) << "buffered_worker_op(" << this << ")::buffered_worker_op";
    }

    ~instance() {
      LOG_S(5) << "buffered_worker_op(" << this << ")::~buffered_worker_op";
      tele::counter_delete(_dropped_items);
      tele::counter_delete(_delivered_items);
      tele::gauge_delete(_buffer_size);
    }

    void on_next(T &&t) override {
      LOG_S(5) << "buffered_worker_op(" << this << ")::on_next";
      if (!_channel.try_send(next{std::move(t)})) {
        tele::counter_inc(_dropped_items);
      }
      tele::gauge_set(_buffer_size, _channel.size());
      _source_sub->request(1);
    };

    void on_error(std::error_condition ec) override {
      LOG_S(5) << "buffered_worker_op(" << this << ")::on_error";
      _channel.send(error{ec});
      _source_sub = nullptr;
    };

    void on_complete() override {
      LOG_S(5) << "buffered_worker_op(" << this << ")::on_complete";
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
      LOG_S(5) << "buffered_worker_op(" << this << ")::cancel";
      if (_source_sub) {
        _source_sub->cancel();
      }
      _is_active = false;
    }

   private:
    void worker_thread_loop() noexcept {
      while (_is_active) {
        LOG_S(6) << "buffered_worker_op(" << this << ")::worker_thread_loop waiting";
        msg m = _channel.recv();
        LOG_S(6) << "buffered_worker_op(" << this
                 << ")::worker_thread_loop got msg _is_active=" << _is_active;
        if (!_is_active) {
          // we are no longer active after waiting for an item.
          break;
        }

        if (boost::get<subscribe>(&m)) {
          LOG_S(6) << "buffered_worker_op(" << this
                   << ")::worker_thread_loop on_subscribe";
          _sink.on_subscribe(*this);
        } else if (next *n = boost::get<next>(&m)) {
          BOOST_ASSERT(_outstanding > 0);
          LOG_S(6) << "buffered_worker_op(" << this << ")::worker_thread_loop >on_next";
          _sink.on_next(std::move(n->t));
          LOG_S(6) << "buffered_worker_op(" << this << ")::worker_thread_loop <on_next";
          _outstanding--;
          tele::counter_inc(_delivered_items);
        } else if (boost::get<complete>(&m)) {
          LOG_S(6) << "buffered_worker_op(" << this << ")::worker_thread_loop complete";
          _sink.on_complete();
          // break the loop
          _is_active = false;
        } else if (error *e = boost::get<error>(&m)) {
          LOG_S(6) << "buffered_worker_op(" << this << ")::worker_thread_loop error";
          _sink.on_error(e->ec);
          // break the loop
          _is_active = false;
        } else {
          BOOST_ASSERT_MSG(false, "unexpected message");
        }
      }
      LOG_S(5) << "buffered_worker_op(" << this << ")::worker_thread_loop exit";
      _worker_thread->detach();
      delete this;
    }

    std::atomic<bool> _is_active{true};
    streams::subscriber<T> &_sink;
    channel<msg> _channel;
    std::unique_ptr<std::thread> _worker_thread;

    tele::counter *_dropped_items;
    tele::counter *_delivered_items;
    tele::gauge *_buffer_size;

    streams::subscription *_source_sub;
    std::atomic<long> _outstanding{0};
  };

  const std::string _name;
  const size_t _size;
};

inline auto buffered_worker(const std::string &name, size_t buffer_size) {
  return buffered_worker_op(name, buffer_size);
}

}  // namespace video
}  // namespace rtm