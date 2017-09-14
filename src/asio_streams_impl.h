#pragma include once

#include <boost/date_time/posix_time/posix_time.hpp>
#include "error.h"

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
        : _fn(std::move(op._fn)), _sink(sink), _io(op._io) {}

    void on_next(T &&t) override {
      _buffer.push_back(std::move(t));
      _timer->expires_from_now(to_boost(_fn(t)));
      _timer->async_wait(std::bind(&instance::on_timer, this, std::placeholders::_1));
    }

    void on_complete() override {
      _sink.on_complete();
      delete this;
    }

    void on_error(std::error_condition ec) override {
      _sink.on_error(ec);
      delete this;
    }

    void on_subscribe(subscription &src) override {
      _src = &src;
      _sink.on_subscribe(*this);
      _timer.reset(new boost::asio::deadline_timer(_io));
      _src->request(1);
    }

    void cancel() override {
      _src->cancel();
      delete this;
    }

    void request(int n) override { _outstanding += n; }

    void on_timer(const boost::system::error_code &ec) {
      if (ec) {
        _sink.on_error(rtm::video::video_error::AsioError);
        delete this;
        return;
      }

      if (!_outstanding) {
        std::cerr << "sink can't keep up with timer, skipping frame\n";
        return;
      }

      BOOST_ASSERT(!_buffer.empty());
      _sink.on_next(std::move(_buffer.front()));
      _buffer.pop_front();
      _outstanding--;
      BOOST_ASSERT(_outstanding > 0);

      _src->request(1);
    }

    Fn _fn;
    subscriber<T> &_sink;
    boost::asio::io_service &_io;

    std::atomic<long> _outstanding{0};
    subscription *_src;
    std::unique_ptr<boost::asio::deadline_timer> _timer;
    std::deque<T> _buffer;
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
};

}  // namespace asio
}  // namespace streams