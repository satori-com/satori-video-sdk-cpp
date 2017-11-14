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
struct delay_op {
  delay_op(boost::asio::io_service &io, Fn &&fn) : _io(io), _fn(fn) {}

  template <typename T>
  struct instance : subscriber<T>, subscription {
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
      LOG(5) << "delay_op(" << this << "::~delay_op";
    }

    void on_next(T &&t) override {
      CHECK(_active);
      LOG(5) << "delay_op(" << this << ")::on_next";

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
      LOG(5) << "delay_op(" << this << ")::cancel _buffer.size()=" << _buffer.size();
      CHECK(_active);
      _src->cancel();
      _src = nullptr;
      _active = false;
      _cancelled = true;

      if (_buffer.empty()) {
        delete this;
      }
      // otherwise there will be on_timer execution eventually;
    }

    void request(int n) override {
      if (!_active) {
        // we are still draining, ignore request.
        return;
      }
      LOG(5) << "request n=" << n;
      _src->request(n);
    }

    void on_timer() {
      LOG(5) << "delay_op::on_timer _buffer.size()=" << _buffer.size();

      CHECK(!_buffer.empty());
      _sink.on_next(std::move(_buffer.front()));
      _buffer.pop_front();

      if (!_buffer.empty()) {
        schedule_timer();
      } else if (!_active) {
        if (_error) {
          LOG(5) << "delay_op not active, on_error";
          _sink.on_error(_error);
        } else if (!_cancelled) {
          LOG(5) << "delay_op not active, on_complete";
          _sink.on_complete();
        }
        delete this;
      }
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

    Fn _fn;
    subscriber<T> &_sink;
    boost::asio::io_service &_io;

    subscription *_src;
    std::unique_ptr<boost::asio::deadline_timer> _timer;
    std::deque<T> _buffer;
    std::atomic<long> _queued{0};
    std::atomic<bool> _active{true};
    std::atomic<bool> _cancelled{false};
    std::error_condition _error{};
  };

  boost::asio::io_service &_io;
  Fn _fn;
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
                      if (this_frame_time < now) {
                        LOG(WARNING) << "late frame in interval";
                        return std::chrono::milliseconds(0);
                      }

                      s->last_frame = this_frame_time;
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

}  // namespace asio
}  // namespace streams
}  // namespace video
}  // namespace satori
