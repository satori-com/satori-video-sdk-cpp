// go-like channel concurrency synchronization mechanism.
#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <utility>

namespace satori {
namespace video {

template <typename T>
class channel {
 public:
  explicit channel(size_t buffer_size) : _buffer_size(buffer_size) {}

  void send(T &&t) {
    std::unique_lock<std::mutex> lock(_mutex);
    while (_buffer.size() >= _buffer_size) {
      _on_receive.wait(lock);
    }
    _buffer.push_back(std::move(t));
    _on_send.notify_one();
  }

  bool try_send(T &&t) {
    std::lock_guard<std::mutex> guard(_mutex);
    if (_buffer.size() >= _buffer_size) {
      return false;
    }

    _buffer.push_back(std::move(t));
    _on_send.notify_one();
    return true;
  }

  T recv() {
    // todo: shutdown
    std::unique_lock<std::mutex> lock(_mutex);
    while (_buffer.empty()) {
      _on_send.wait(lock);
    }

    T t = std::move(_buffer.front());
    _buffer.pop_front();
    _on_receive.notify_one();
    return std::move(t);
  }

  size_t size() {
    std::lock_guard<std::mutex> guard(_mutex);
    return _buffer.size();
  }

  void clear() {
    std::lock_guard<std::mutex> guard(_mutex);
    _buffer.clear();
  }

 private:
  std::mutex _mutex;
  size_t _buffer_size;
  std::deque<T> _buffer;

  std::condition_variable _on_send;
  std::condition_variable _on_receive;
};

}  // namespace video
}  // namespace satori
