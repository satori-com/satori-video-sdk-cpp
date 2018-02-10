#pragma once

#include <boost/variant.hpp>
#include <thread>

#include "../metrics.h"
#include "../threadutils.h"

#include "channel.h"
#include "streams.h"

namespace satori {
namespace video {
namespace streams {

namespace impl {

class threaded_worker_op {
 public:
  explicit threaded_worker_op(const std::string &name) : _name(name) {}

  template <typename T>
  class instance : publisher_impl<std::queue<T>> {
    using element_t = std::queue<T>;

    class source : drain_source_impl<element_t>, subscriber<T> {
     public:
      source(const std::string &name, publisher<T> &&src,
             streams::subscriber<element_t> &sink)
          : _name(name), drain_source_impl<element_t>(sink) {
        _worker_thread = std::make_unique<std::thread>(&source::worker_thread_loop, this);

        while (!_worker_thread_ready) {
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        src->subscribe(*this);
      }

      ~source() override {
        LOG(INFO) << this << " " << _name << " ~source";
        CHECK(std::this_thread::get_id() == _worker_thread->get_id());
        CHECK(!_thread_should_be_active);

        if (_src) {
          LOG(INFO) << this << " " << _name << " cancelling upstream";
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
        CHECK_NOTNULL(_src) << this << " " << _name;
        _buffer.emplace(std::move(t));
        _on_send.notify_one();
      }

      void on_error(std::error_condition ec) override {
        std::lock_guard<std::mutex> guard(_mutex);
        LOG(5) << this << " " << _name << " on_error: " << ec.message();
        CHECK_NOTNULL(_src) << this << " " << _name;
        _src = nullptr;
        _ec = ec;
        _thread_should_be_active = false;
        _on_send.notify_one();
      }

      void on_complete() override {
        std::lock_guard<std::mutex> guard(_mutex);
        LOG(5) << this << " " << _name << " on_complete";
        CHECK_NOTNULL(_src) << this << " " << _name;
        _src = nullptr;
        _complete = true;
        _thread_should_be_active = false;
        _on_send.notify_one();
      }

      void worker_thread_loop() noexcept {
        threadutils::set_current_thread_name(_name);
        LOG(INFO) << this << " " << _name << " started worker thread";
        drain_source_impl<element_t>::deliver_on_subscribe();

        while (_thread_should_be_active) {
          {
            std::unique_lock<std::mutex> lock(_mutex);
            _worker_thread_ready = true;
            while (_thread_should_be_active && _buffer.empty()) {
              LOG(5) << this << " " << _name << " waiting for _on_send";
              _on_send.wait(lock);
            }

            if (_buffer.empty() || !(_thread_should_be_active || _complete || _ec)) {
              break;
            }
          }

          drain_source_impl<element_t>::drain();
        }

        std::string finish_reason;
        if (_cancelled) {
          finish_reason = "cancelled";
        } else if (_complete) {
          finish_reason = "complete";
        } else if (_ec) {
          finish_reason = _ec.message();
        } else {
          ABORT() << "unreachable code";
        }
        LOG(INFO) << this << " " << _name
                  << " finished worker thread loop: " << finish_reason;

        _worker_thread_ready = true;

        if (!_cancelled) {
          if (_complete) {
            drain_source_impl<element_t>::deliver_on_complete();
          } else if (_ec) {
            drain_source_impl<element_t>::deliver_on_error(_ec);
          }
        }

        LOG(INFO) << this << " " << _name << " destroying thread_worker";
        delete this;
      }

      void die() override {
        LOG(INFO) << this << " " << _name << " die() from "
                  << threadutils::get_current_thread_name();
        _thread_should_be_active = false;
      }

      void cancel() override {
        LOG(INFO) << this << " " << _name << " cancel() from "
                  << threadutils::get_current_thread_name();
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
        LOG(5) << this << " " << _name << " drain_impl " << _buffer.size();
        std::queue<T> tmp;

        {
          std::unique_lock<std::mutex> lock(_mutex);
          if (_buffer.empty()) {
            return false;
          }
          _buffer.swap(tmp);
        }

        LOG(5) << this << " " << _name << " delivering batch: " << tmp.size();
        drain_source_impl<element_t>::deliver_on_next(std::move(tmp));
        return false;
      }

      std::atomic_bool _worker_thread_ready{false};
      const std::string _name;
      std::mutex _mutex;
      std::condition_variable _on_send;

      std::atomic_bool _complete{false};
      bool _cancelled{false};
      std::error_condition _ec;

      std::queue<T> _buffer;
      std::unique_ptr<std::thread> _worker_thread;
      std::atomic_bool _thread_should_be_active{true};
      subscription *_src{nullptr};
    };

   public:
    static publisher<std::queue<T>> apply(publisher<T> &&src, threaded_worker_op &&op) {
      return publisher<std::queue<T>>(new instance(op._name, std::move(src)));
    }

    instance(const std::string &name, publisher<T> &&src)
        : _name(name), _src(std::move(src)) {}

    void subscribe(subscriber<element_t> &s) override {
      new source(_name, std::move(_src), s);
    }

   private:
    const std::string _name;
    publisher<T> _src;
  };

 private:
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
