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

struct threaded_worker_op {
  explicit threaded_worker_op(const std::string &name) : _name(name) {}

  template <typename T>
  struct instance : publisher_impl<std::queue<T>> {
    using element_t = std::queue<T>;

    struct source : drain_source_impl<element_t>, subscriber<T> {
      source(publisher<T> &&src, streams::subscriber<element_t> &sink)
          : drain_source_impl<element_t>(sink) {
        _worker_thread = std::make_unique<std::thread>(&source::worker_thread_loop, this);
        src->subscribe(*this);
      }

      ~source() override {
        LOG(5) << this << " ~source";
        CHECK(std::this_thread::get_id() == _worker_thread->get_id());
        CHECK(!_active);

        if (_src) {
          _src->cancel();
          _src = nullptr;
        }
        _worker_thread->detach();
      }

     private:
      void on_subscribe(subscription &s) override {
        _src = &s;
        _src->request(INT_MAX);
      }

      void on_next(T &&t) override {
        std::lock_guard<std::mutex> guard(_mutex);
        CHECK(_active);
        _buffer.emplace(std::move(t));
        _on_send.notify_one();
      }

      void on_error(std::error_condition ec) override {
        std::lock_guard<std::mutex> guard(_mutex);
        LOG(5) << this << " on_error: " << ec.message();
        CHECK(_active);
        _src = nullptr;
        _ec = ec;
        _active = false;
        _on_send.notify_one();
      }

      void on_complete() override {
        std::lock_guard<std::mutex> guard(_mutex);
        LOG(5) << this << " on_complete";
        CHECK(_active);
        _src = nullptr;
        _complete = true;
        _active = false;
        _on_send.notify_one();
      }

      void worker_thread_loop() noexcept {
        LOG(INFO) << this << " started worker thread";
        drain_source_impl<element_t>::deliver_on_subscribe();

        while (_active) {
          {
            std::unique_lock<std::mutex> lock(_mutex);
            while (_active && _buffer.empty()) {
              LOG(5) << this << " waiting for _on_send";
              _on_send.wait(lock);
            }

            if (_buffer.empty() || !(_active || _complete || _ec)) {
              break;
            }
          }

          drain_source_impl<element_t>::drain();
        }

        LOG(2) << this << " finished worker thread loop: "
               << (_complete ? "complete" : _ec.message());

        if (!_cancelled) {
          if (_complete) {
            drain_source_impl<element_t>::deliver_on_complete();
          } else if (_ec) {
            drain_source_impl<element_t>::deliver_on_error(_ec);
          }
        }
      }

      void die() override {
        LOG(2) << this << " die";
        CHECK(std::this_thread::get_id() == _worker_thread->get_id());
        _active = false;
      }

      void cancel() override {
        _cancelled = true;
        drain_source_impl<element_t>::cancel();
      }

      bool drain_impl() override {
        if (!_worker_thread) {
          return true;
        }
        if (std::this_thread::get_id() != _worker_thread->get_id()) {
          // drain only on worker thread
          return false;
        }
        LOG(5) << this << " drain_impl " << _buffer.size();
        std::queue<T> tmp;

        {
          std::unique_lock<std::mutex> lock(_mutex);
          if (_buffer.empty()) {
            return false;
          }
          _buffer.swap(tmp);
        }

        LOG(5) << this << " delivering batch: " << tmp.size();
        drain_source_impl<element_t>::deliver_on_next(std::move(tmp));
        return false;
      }

      std::mutex _mutex;
      std::condition_variable _on_send;

      bool _active{true};
      bool _complete{false};
      bool _cancelled{false};
      std::error_condition _ec;

      std::queue<T> _buffer;
      std::unique_ptr<std::thread> _worker_thread;
      subscription *_src;
    };

    static publisher<std::queue<T>> apply(publisher<T> &&src,
                                          threaded_worker_op && /*op*/) {
      return publisher<std::queue<T>>(new instance(std::move(src)));
    }

    instance(publisher<T> &&src) : _src(std::move(src)) {}

    void subscribe(subscriber<element_t> &s) override { new source(std::move(_src), s); }

   private:
    publisher<T> _src;
  };

  const std::string _name;
};

}  // namespace impl

// threaded worker transforms publisher<T> into publisher<std::queue<T>> by
// spawning new thread and performing all element delivery in it.
inline auto threaded_worker(const std::string &name) {
  return impl::threaded_worker_op(name);
}

}  // namespace streams
}  // namespace video
}  // namespace satori
