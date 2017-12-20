#pragma include once

#include <boost/date_time/posix_time/posix_time.hpp>
#include "../logging.h"
#include "stream_error.h"

namespace satori {
namespace video {

namespace streams {
namespace asio {

namespace impl {

template <class Rep, class Period>
inline boost::posix_time::time_duration to_boost(
    const std::chrono::duration<Rep, Period> &dur) {
  using duration_t = std::chrono::nanoseconds;
  using rep_t = duration_t::rep;
  rep_t d = std::chrono::duration_cast<duration_t>(dur).count();
  rep_t sec = d / 1000000000;
  rep_t nsec = d % 1000000000;
  return boost::posix_time::seconds(static_cast<long>(sec)) +
#ifdef BOOST_DATE_TIME_HAS_NANOSECONDS
         boost::posix_time::nanoseconds(nsec);
#else
         boost::posix_time::microseconds((nsec + 500) / 1000);
#endif
}

template <typename Fn>
class delay_op {
 public:
  delay_op(boost::asio::io_service &io, Fn &&fn) : _io(io), _fn(fn) {}

  template <typename T>
  class instance : public subscriber<T>, subscription {
   public:
    using value_t = T;

    static publisher<T> apply(publisher<T> &&src, delay_op &&op) {
      return publisher<T>(
          new streams::impl::op_publisher<T, T, delay_op>(std::move(src), std::move(op)));
    }

    instance(delay_op &&op, subscriber<T> &sink)
        : _fn(std::move(op._fn)), _sink(sink), _io(op._io) {
      LOG(5) << "delay_op(" << this << ")";
    }

    ~instance() override {
      _timer.reset();
      LOG(5) << "delay_op(" << this << ")::~delay_op";
    }

   private:
    void on_next(T &&t) override {
      CHECK(_active);
      CHECK_GT(_waiting_from_src, 0);
      LOG(5) << "delay_op(" << this << ")::on_next";

      _waiting_from_src--;
      _buffer.push_back(std::move(t));

      if (_buffer.size() == 1) {
        schedule_timer();
      }
    }

    void on_complete() override {
      CHECK(_active);
      LOG(5) << "delay_op(" << this << ")::on_complete _buffer.size()=" << _buffer.size();
      if (_buffer.empty()) {
        _sink.on_complete();
        delete this;
      } else {
        _active = false;
      }
    }

    void on_error(std::error_condition ec) override {
      CHECK(_active);
      LOG(5) << "delay_op(" << this << ")::on_error _buffer.size()=" << _buffer.size();
      if (_buffer.empty()) {
        _sink.on_error(ec);
        delete this;
      } else {
        _active = false;
        _error = ec;
      }
    }

    void on_subscribe(subscription &src) override {
      _src = &src;
      _timer = std::make_unique<boost::asio::deadline_timer>(_io);
      _sink.on_subscribe(*this);
    }

    void cancel() override {
      LOG(5) << "delay_op(" << this << ")::cancel _active=" << _active
             << " _buffer.size()=" << _buffer.size();

      // the source might have already completed, but we haven't.
      if (_active) {
        _src->cancel();
        _src = nullptr;
        _active = false;
      }
      _cancelled = true;

      if (_buffer.empty()) {
        delete this;
      }
      // otherwise there will be on_timer execution eventually;
    }

    void request(int n) override {
      CHECK_GT(n, 0);

      if (!_active) {
        // we are still draining, ignore request.
        return;
      }
      LOG(5) << "delay_op(" << this << ")::request n=" << n;

      CHECK_LE(n, INT_MAX - _sink_needs);
      _sink_needs += n;

      maybe_request();
    }

    void on_timer() {
      LOG(5) << "delay_op(" << this << ")::on_timer _cancelled=" << _cancelled
             << " _buffer.size()=" << _buffer.size() << " _sink_needs=" << _sink_needs;
      if (_cancelled) {
        delete this;
        return;
      }

      CHECK(!_buffer.empty());
      CHECK_GT(_sink_needs, 0);
      _sink_needs--;
      _sink.on_next(std::move(_buffer.front()));
      _buffer.pop_front();

      // we can be cancelled as a result of on_next call.
      if (_cancelled) {
        delete this;
        return;
      }

      if (!_buffer.empty()) {
        schedule_timer();
        return;
      }

      if (_active) {
        maybe_request();
        return;
      }

      if (_error) {
        LOG(5) << "delay_op not active, on_error";
        _sink.on_error(_error);
      } else if (!_cancelled) {
        LOG(5) << "delay_op not active, on_complete";
        _sink.on_complete();
      }
      delete this;
    }

    void schedule_timer() {
      CHECK(!_buffer.empty());

      auto delay = _fn(_buffer.front());
      LOG(5) << "delay_op(" << this << ")::schedule_timer delay=" << delay.count();

      if (delay.count() == 0) {
        on_timer();
      } else {
        _timer->expires_from_now(to_boost(delay));
        _timer->async_wait([this](const boost::system::error_code &ec) {
          if (ec.value() != 0) {
            LOG(ERROR) << "ASIO ERROR: " << ec.message();
            return;
          }
          on_timer();
        });
      }
    }

    void maybe_request() {
      CHECK_GE(_sink_needs, 0);
      CHECK_GE(_waiting_from_src, 0);
      LOG(5) << "delay_op(" << this << ")::maybe_request _sink_needs=" << _sink_needs
             << ", _buffer.size()=" << _buffer.size()
             << ", _waiting_from_src=" << _waiting_from_src;

      if (_sink_needs == 0) {
        LOG(5) << "delay_op(" << this << ")::maybe_request sink doesn't need anything";
        return;
      }

      if (_buffer.size() + _waiting_from_src >= _buffer_low_watermark) {
        LOG(5) << "delay_op(" << this << ")::maybe_request enough items";
        return;
      }

      int requesting = std::min(
          _buffer_low_watermark - (int)_buffer.size() - _waiting_from_src, _sink_needs);
      CHECK_GT(requesting, 0);
      LOG(5) << "delay_op(" << this << ")::maybe_request requesting " << requesting;
      _waiting_from_src += requesting;
      _src->request(requesting);
    }

    Fn _fn;
    subscriber<T> &_sink;
    boost::asio::io_service &_io;

    subscription *_src;
    std::unique_ptr<boost::asio::deadline_timer> _timer;
    std::deque<T> _buffer;
    std::atomic<bool> _active{true};
    std::atomic<bool> _cancelled{false};
    std::error_condition _error{};

    const int _buffer_low_watermark{20};
    int _sink_needs{0};
    int _waiting_from_src{0};
  };

 private:
  boost::asio::io_service &_io;
  Fn _fn;
};

class timeout_op {
 public:
  timeout_op(boost::asio::io_service &io, std::chrono::milliseconds time)
      : _io(io), _time(time) {}

  template <typename T>
  class instance : public subscriber<T>, subscription {
   public:
    using value_t = T;

    static publisher<T> apply(publisher<T> &&src, timeout_op &&op) {
      return publisher<T>(new streams::impl::op_publisher<T, T, timeout_op>(
          std::move(src), std::move(op)));
    }

    instance(timeout_op &&op, subscriber<T> &sink)
        : _io(op._io), _time(op._time), _sink(sink) {
      LOG(5) << "timeout_op(" << this << ")";
    }

   private:
    void request(int n) override {
      LOG(5) << "timeout_op(" << this << ")::request " << n;
      CHECK(_src);
      arm_timer();
      _src->request(n);
    }

    void cancel() override {
      LOG(5) << "timeout_op(" << this << ")::cancel";
      CHECK(_src);
      _src->cancel();
      _src = nullptr;
      delete this;
    }

    void on_next(T &&t) override {
      LOG(5) << "timeout_op(" << this << ")::on_next";
      _sink.on_next(std::move(t));
      arm_timer();
    }

    void on_error(std::error_condition ec) override {
      LOG(5) << "timeout_op(" << this << ")::on_error";
      CHECK(_src);
      _src = nullptr;
      _sink.on_error(ec);
      delete this;
    }

    void on_complete() override {
      LOG(5) << "timeout_op(" << this << ")::on_complete";
      CHECK(_src);
      _src = nullptr;
      _sink.on_complete();
      delete this;
    }

    void on_subscribe(subscription &src) override {
      LOG(5) << "timeout_op(" << this << ")::on_subscribe";
      CHECK(!_src);
      _src = &src;
      _timer = std::make_unique<boost::asio::deadline_timer>(_io);
      _sink.on_subscribe(*this);
    }

    void arm_timer() {
      LOG(5) << "timeout_op(" << this << ")::arm_timer";
      CHECK(_timer);
      _timer->cancel();
      _timer->expires_from_now(to_boost(_time));
      _timer->async_wait([this](const boost::system::error_code &ec) {
        if (ec.value() != 0) {
          if (ec.value() == boost::system::errc::operation_canceled) {
            LOG(5) << "timeout_op(" << this << ") timer operation cancelled";
          } else {
            LOG(ERROR) << "ASIO ERROR: " << ec.message();
            send_error_downstream(stream_error::ASIO_ERROR);
          }
          return;
        }
        LOG(1) << "timeout_op(" << this << ") timeout detected, sending error upstream";
        send_error_downstream(stream_error::TIMEOUT);
      });
    }

    void send_error_downstream(std::error_condition ec) {
      CHECK(_src);
      _src->cancel();
      _src = nullptr;
      _sink.on_error(ec);
      delete this;
    }

    boost::asio::io_service &_io;
    const std::chrono::milliseconds _time;
    subscriber<T> &_sink;
    subscription *_src{nullptr};
    std::unique_ptr<boost::asio::deadline_timer> _timer;
  };

 private:
  boost::asio::io_service &_io;
  const std::chrono::milliseconds _time;
};

}  // namespace impl

template <typename Fn>
auto delay(boost::asio::io_service &io, Fn &&fn) {
  return impl::delay_op<Fn>(io, std::forward<Fn>(fn));
}

template <typename T>
streams::op<T, T> interval(boost::asio::io_service &io,
                           std::chrono::milliseconds period) {
  return [&io, period](publisher<T> &&src) {
    struct state {
      std::chrono::system_clock::time_point last_frame;
    };

    auto s = new state();
    return std::move(src)
           >> delay(io,
                    [s, period](const T &t) {
                      if (!s->last_frame.time_since_epoch().count()) {
                        s->last_frame = std::chrono::system_clock::now();
                        return std::chrono::milliseconds(0);
                      }

                      auto this_frame_time = s->last_frame + period;
                      auto now = std::chrono::system_clock::now();
                      s->last_frame = this_frame_time;

                      if (this_frame_time < now) {
                        LOG(WARNING) << "late frame in interval";
                        return std::chrono::milliseconds(0);
                      }

                      return std::chrono::duration_cast<std::chrono::milliseconds>(
                          this_frame_time - now);
                    })
           >> streams::do_finally([s]() { delete s; });
  };
}

template <typename T>
streams::op<T, T> timer_breaker(boost::asio::io_service &io,
                                std::chrono::milliseconds time) {
  return [&io, time](publisher<T> &&src) {
    auto timer = new boost::asio::deadline_timer(io);
    auto flag = new std::atomic<bool>{true};

    timer->expires_from_now(impl::to_boost(time));
    timer->async_wait(
        [flag](const boost::system::error_code &ec) { flag->store(false); });

    return std::move(src) >> take_while([flag, timer](const T &) {
             bool result = flag->load();
             if (!result) {
               LOG(INFO) << "time limit expired, breaking the stream";
               delete timer;
               delete flag;
             }
             return result;
           });
  };
}
template <typename T>
auto timeout(boost::asio::io_service &io, std::chrono::milliseconds time) {
  return impl::timeout_op(io, time);
}

}  // namespace asio
}  // namespace streams
}  // namespace video
}  // namespace satori
