#pragma once

#include <deque>
#include <mutex>

template <typename T>
struct producer_consumer_queue {
 public:
  void put(T &&t) {
    std::lock_guard<std::mutex> lock{_mutex};
    _queue.push_back(std::move(t));
  }

  bool poll(T &t) {
    std::lock_guard<std::mutex> lock{_mutex};
    if (_queue.empty()) {
      return false;
    }

    t = std::move(_queue.front());
    _queue.pop_front();
    return true;
  }

 private:
  std::mutex _mutex;
  std::deque<T> _queue;
};