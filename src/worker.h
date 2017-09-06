#pragma once

#include <boost/variant.hpp>
#include <cassert>
#include <functional>
#include <thread>

#include "librtmvideo/tele.h"
#include "channel.h"
#include "streams.h"

namespace rtm {
namespace video {

template <typename T>
class threaded_worker {
 public:
  using callback_t = std::function<void(T &&)>;

  threaded_worker(size_t buffer_size, callback_t &&callback)
      : _channel(std::make_unique<channel<T>>(buffer_size)),
        _callback(std::move(callback)) {
    _worker_thread =
        std::make_unique<std::thread>(&threaded_worker::thread_loop, this);
  }

  ~threaded_worker() {
    _should_exit = true;
    _channel->try_send(T{});
    _worker_thread->join();
  }

  bool try_send(T &&t) noexcept {
    if (_should_exit) {
      return false;
    }

    return _channel->try_send(std::move(t));
  }

  void send(T &&t) noexcept {
    _channel->send(std::move(t));
  }

  size_t queue_size() noexcept { return _channel->size(); }

  void clear() noexcept { _channel->clear(); }

 private:
  void thread_loop() noexcept {
    while (true) {
      T t = _channel->recv();
      if (_should_exit) {
        return;
      }
      _callback(std::move(t));
    }
  }

  std::atomic_bool _should_exit{false};
  std::unique_ptr<channel<T>> _channel;
  std::unique_ptr<std::thread> _worker_thread;
  callback_t _callback;
};


// streams operator.
struct buffered_worker_op {
  buffered_worker_op(const std::string &name, size_t buffer_size)
      : _name(name), _size(buffer_size) {}

  template<typename T>
  struct instance : streams::subscriber<T>, streams::subscription {
    using value_t = T;

    struct subscribe{};
    struct complete{};
    struct error{std::error_condition ec;};
    struct next{T t;};
    using msg = boost::variant<subscribe, next, error, complete>;

    static streams::publisher<T> apply(streams::publisher<T> &&src, buffered_worker_op &&op) {
      return streams::publisher<T>(
          new streams::impl::op_publisher<T, T, buffered_worker_op>(std::move(src), std::move(op)));
    }

    instance(buffered_worker_op &&op, streams::subscriber<T> &sink)
        : _sink(sink) {
      _worker.reset(new rtm::video::threaded_worker<msg>(op._size, [this](msg&& m) {
        on_msg(std::move(m));
      }));
      _dropped_items = tele::counter_new(op._name.c_str(), "dropped");
      _delivered_items = tele::counter_new(op._name.c_str(), "delivered");
    }

    ~instance() {
      tele::counter_delete(_dropped_items);
      tele::counter_delete(_delivered_items);
    }

    void on_next(T &&t) override {
      if (!_worker->try_send(next{std::move(t)})) {
        tele::counter_inc(_dropped_items);
      }
      _source_sub->request(1);
    };

    void on_error(std::error_condition ec) override {
      _worker->send(error{ec});
    };

    void on_complete() override {
      _worker->send(complete{});
    };

    void on_subscribe(subscription &s) override {
      _source_sub = &s;
      _worker->send(subscribe{});
      _source_sub->request(1);
    };

    void request(int n) override {
      _outstanding += n;
    }

    void cancel() override {
      BOOST_ASSERT_MSG(false, "not implemented");
    }

    void on_msg(msg &&m) {
      if (boost::get<subscribe>(&m)) {
        _sink.on_subscribe(*this);
      } else if (next* n = boost::get<next>(&m)) {
        BOOST_ASSERT(_outstanding > 0);
        _sink.on_next(std::move(n->t));
        _outstanding--;
        tele::counter_inc(_delivered_items);
      } else if (boost::get<complete>(&m)) {
        _sink.on_complete();
        delete this;
      } else if (error *e = boost::get<error>(&m)) {
        _sink.on_error(e->ec);
        delete this;
      } else {
        BOOST_ASSERT_MSG(false, "unexpected message");
      }
    }

    streams::subscriber<T> &_sink;
    std::unique_ptr<rtm::video::threaded_worker<msg>> _worker;
    tele::counter *_dropped_items;
    tele::counter *_delivered_items;
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