#pragma once

#include <cassert>
#include <functional>
#include <thread>

#include "channel.h"

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

}  // namespace video
}  // namespace rtm