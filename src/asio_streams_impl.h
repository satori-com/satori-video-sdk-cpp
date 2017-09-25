#pragma include once

#include <boost/date_time/posix_time/posix_time.hpp>
#include "error.h"
#include "logging.h"

namespace streams {
namespace asio {

namespace impl {

template <class Rep, class Period>
inline boost::posix_time::time_duration to_boost(
    const std::chrono::duration<Rep, Period> &dur) {
  typedef std::chrono::nanoseconds duration_t;
  typedef duration_t::rep rep_t;
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
  struct instance : public subscriber<T>, subscription {
    using value_t = T;

    static publisher<T> apply(publisher<T> &&src, delay_op &&op) {
      return publisher<T>(new ::streams::impl::op_publisher<T, T, delay_op>(
          std::move(src), std::move(op)));
    }

    instance(delay_op &&op, subscriber<T> &sink)
        : _fn(std::move(op._fn)), _sink(sink), _io(op._io) {
      LOG_S(5) << "delay_op(" << this << ")";
    }

    ~instance() {
      _timer.reset();
      LOG_S(5) << "delay_op(" << this << "::~delay_op";
    }

    void on_next(T &&t) override {
      BOOST_ASSERT(_active);
      auto delay = _fn(t);
      LOG_S(5) << "delay_op(" << this << ")::on_next delay=" << delay.count();

      if (delay.count()) {
        _buffer.push_back(std::move(t));
        _timer->expires_from_now(to_boost(delay));
        _timer->async_wait(std::bind(&instance::on_timer, this, std::placeholders::_1));
      } else {
        _sink.on_next(std::move(t));
      }
    }

    void on_complete() override {
      BOOST_ASSERT(_active);
      LOG_S(5) << "delay_op(" << this
               << ")::on_complete _buffer.size()=" << _buffer.size();
      if (_buffer.empty()) {
        _sink.on_complete();
        delete this;
      } else {
        _active = false;
      }
    }

    void on_error(std::error_condition ec) override {
      BOOST_ASSERT(_active);
      LOG_S(5) << "delay_op(" << this << ")::on_error _buffer.size()=" << _buffer.size();
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
      _timer.reset(new boost::asio::deadline_timer(_io));
      _sink.on_subscribe(*this);
    }

    void cancel() override {
      LOG_S(5) << "delay_op(" << this << ")::cancel _buffer.size()=" << _buffer.size();
      BOOST_ASSERT(_active);
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
      LOG_S(5) << "request n=" << n;
      _src->request(n);
    }

    void on_timer(const boost::system::error_code &ec) {
      LOG_S(5) << "delay_op::on_timer _buffer.size()=" << _buffer.size();

      if (ec) {
        LOG_S(ERROR) << "ASIO ERROR: " << ec.message();
      }

      BOOST_ASSERT(!_buffer.empty());
      _sink.on_next(std::move(_buffer.front()));
      _buffer.pop_front();

      if (!_active && _buffer.empty()) {
        if (_error) {
          LOG_S(5) << "delay_op not active, on_error";
          _sink.on_error(_error);
        } else if (!_cancelled) {
          LOG_S(5) << "delay_op not active, on_complete";
          _sink.on_complete();
        }
        delete this;
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
  return impl::delay_op<Fn>(io, std::move(fn));
}

template <typename T>
streams::op<T, T> interval(boost::asio::io_service &io,
                           std::chrono::milliseconds period) {
  return [&io, period](publisher<T> &&src) {
    struct state {
      std::chrono::system_clock::time_point last_frame;
    };

    state *s = new state();
    return std::move(src)
           >> delay(io,
                    [s, period](const T &t) {
                      if (!s->last_frame.time_since_epoch().count())
                        return std::chrono::milliseconds(0);

                      auto this_frame_time = s->last_frame + period;
                      auto now = std::chrono::system_clock::now();
                      if (this_frame_time < now) {
                        std::cerr << "late frame in interval\n";
                        return std::chrono::milliseconds(0);
                      }

                      return std::chrono::duration_cast<std::chrono::milliseconds>(
                          this_frame_time - now);
                    })
           >> streams::map([s](T &&t) {
               s->last_frame = std::chrono::system_clock::now();
               return std::move(t);
             })
           >> streams::do_finally([s]() { delete s; });
  };
}

template <typename T>
streams::op<T, T> timer_breaker(boost::asio::io_service &io,
                                std::chrono::milliseconds time) {
  return [&io, time](publisher<T> &&src) {
    boost::asio::deadline_timer *timer = new boost::asio::deadline_timer(io);
    std::atomic<bool> *flag = new std::atomic<bool>{true};

    timer->expires_from_now(impl::to_boost(time));
    timer->async_wait(
        [flag](const boost::system::error_code &ec) { flag->store(false); });

    return std::move(src) >> take_while([flag, timer](const T &) {
             bool result = flag->load();
             if (!result) {
               LOG_S(INFO) << "time limit expired, breaking the stream";
               delete timer;
               delete flag;
             }
             return result;
           });
  };
}

}  // namespace asio
}  // namespace streams