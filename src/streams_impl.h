#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace streams {

namespace impl {

template <typename T>
struct async_publisher_impl : public publisher_impl<T> {
  using init_fn_t = std::function<void(observer<T> &observer)>;

  explicit async_publisher_impl(init_fn_t init_fn) : _init_fn(init_fn) {}

  struct sub : subscription, public observer<T> {
    subscriber<T> &_sink;
    std::atomic<long> outstanding;

    explicit sub(subscriber<T> &sink) : _sink(sink) {}

    virtual void request(long n) { outstanding += n; }

    virtual void cancel() { BOOST_ASSERT_MSG(false, "not implemented"); }

    virtual void on_next(const T &t) {
      if (outstanding.fetch_sub(1) <= 0) {
        std::cerr << "can't keep up with async source, dropping data\n";
        outstanding++;
        return;
      }

      _sink.on_next(t);
    }

    virtual void on_error(const std::string &message) {
      BOOST_ASSERT_MSG(false, "not implemented");
    }

    virtual void on_complete() {
      BOOST_ASSERT_MSG(false, "not implemented");
    }
  };

  virtual void subscribe(subscriber<T> &s) {
    auto inst = new sub(s);
    _init_fn(*inst);
    s.on_subscribe(*inst);
  }

  init_fn_t _init_fn;
};

template <typename T>
struct empty_publisher : public publisher_impl<T> {
  void subscribe(subscriber<T> &s) override { s.on_complete(); }
};

template <typename T>
publisher<T> of_impl(std::initializer_list<T> values) {
  std::vector<T> data(values);
  return publishers<T>::template generate<size_t>(
      {.create_fn =
           []() {
             size_t *idx = new size_t;
             *idx = 0;
             return idx;
           },
       .gen_fn =
           [data](size_t *idx, long n, observer<T> &sink) {
             for (int i = 0; i < n && *idx < data.size(); ++i, ++*idx) {
               sink.on_next(data[*idx]);
             }

             if (*idx == data.size()) {
               sink.on_complete();
             }
           }});
}

template <typename T>
publisher<T> range_impl(T from, T to) {
  return publishers<T>::template generate<T>(
      {.create_fn =
           [from]() {
             T *t = new T();
             *t = from;
             return t;
           },
       .gen_fn =
           [to](T *t, long n, observer<T> &sink) {
             for (int i = 0; i < n && *t < to; ++i, ++*t) {
               sink.on_next(*t);
             }

             if (*t == to) {
               sink.on_complete();
             }
           }});
}

struct interval_op {};

inline auto interval(std::chrono::milliseconds i) { return interval_op(); }

template <typename T, typename State>
struct generator_publisher : public publisher_impl<T> {
  using value_t = T;

  struct sub : public subscription, observer<T> {
    const generator<T, State> _gen;
    subscriber<T> &_sink;

    std::unique_ptr<State> _state;
    bool _active{true};
    bool _in_request{false};
    long _outstanding{0};

    explicit sub(generator<T, State> gen, subscriber<T> &sink)
        : _gen(gen), _sink(sink), _state(_gen.create_fn()) {}

    void request(int n) override {
      BOOST_ASSERT(_active);

      _outstanding += n;

      if (_in_request) {
        // this is recursive call
        return;
      }

      while (_active && _outstanding > 0) {
        _in_request = true;
        long k = _outstanding;
        _gen.gen_fn(_state.get(), k, *this);
        _in_request = false;
      }
    }

    void cancel() override { BOOST_ASSERT_MSG(false, "not implemented"); }

    void on_next(const T &t) override {
      _outstanding--;
      _sink.on_next(t);
    }
    void on_error(const std::string &message) override {
      _sink.on_error(message);
      _active = false;
    }

    void on_complete() override {
      _sink.on_complete();
      _active = false;
    }
  };

  explicit generator_publisher(generator<T, State> gen) : _gen(gen) {}

  void subscribe(subscriber<value_t> &s) override {
    s.on_subscribe(*(new sub(_gen, s)));
  }

  generator<T, State> _gen;
};

// operators -------

template <typename S, typename T, typename Fn>
struct map_op {
  using value_t = T;

  explicit map_op(Fn &&fn) : _fn(fn) {}

  struct source_sub : public subscriber<S>, private subscription {
    subscriber<T> &_sink;
    Fn _fn;
    subscription *_source{nullptr};

    explicit source_sub(Fn &&fn, subscriber<T> &sink)
        : _fn(std::move(fn)), _sink(sink) {}

    void on_next(const S &t) override { _sink.on_next(_fn(t)); }

    void on_error(const std::string &message) override {
      BOOST_ASSERT_MSG(false, "not implemented");
    }

    void on_complete() override { _sink.on_complete(); }

    void on_subscribe(subscription &s) override {
      BOOST_ASSERT(!_source);
      _source = &s;
      _sink.on_subscribe(*this);
    }

    void request(int n) override { _source->request(n); }

    void cancel() override { BOOST_ASSERT_MSG(false, "not implemented"); }
  };

  void subscribe(publisher<S> &source, subscriber<value_t> &sink) {
    source->subscribe(*(new source_sub(std::move(_fn), sink)));
  }

  Fn _fn;
};

template <typename S, typename T, typename Fn>
struct flat_map_op {
  using value_t = T;

  explicit flat_map_op(Fn &&fn) : _fn(std::move(fn)) {}

  struct source_sub : public subscriber<S>, subscription {
    subscriber<value_t> &_sink;
    Fn _fn;
    subscription *_source{nullptr};
    bool _in_drain{false};
    bool _active{true};
    bool _requested_next{false};
    std::atomic<long> _outstanding{0};

    publisher<T> _current_result;

    source_sub(subscriber<value_t> &sink, Fn fn) : _sink(sink), _fn(fn) {}

    void on_subscribe(subscription &s) override {
      BOOST_ASSERT(!_source);
      _source = &s;
      _sink.on_subscribe(*this);
    }

    void on_next(const S &t) override {
      BOOST_ASSERT(!_current_result);
      _requested_next = false;
      _current_result = _fn(t);
      _current_result->subscribe(*(new fwd_sub(_sink, this)));
      drain();
    }

    void on_error(const std::string &message) override {
      _active = false;
      _sink.on_error(message);
    }

    void on_complete() override {
      _active = false;
      _sink.on_complete();
    }

    void request(int n) override {
      _outstanding += n;
      drain();
    }

    void drain() {
      if (!_active || !_outstanding || _in_drain) {
        return;
      }

      _in_drain = true;
      while (_active && _outstanding && !_current_result) {
        _requested_next = true;
        _source->request(1);
        if (!_current_result && _requested_next) break;
      }
      _in_drain = false;
    }

    void cancel() override { BOOST_ASSERT_MSG(false, "not implemented"); }

    void current_result_complete() {
      _current_result.reset();
      drain();
    }
  };

  struct fwd_sub : public subscriber<T> {
    subscriber<value_t> &_sink;
    source_sub *_source_sub;
    subscription *_sub{nullptr};
    explicit fwd_sub(subscriber<value_t> &sink, source_sub *s)
        : _sink(sink), _source_sub(s) {}

    void on_subscribe(subscription &s) override {
      BOOST_ASSERT(!_sub);
      _sub = &s;
      _sub->request(_source_sub->_outstanding);
    }

    void on_next(const T &t) override {
      _source_sub->_outstanding--;
      _sink.on_next(t);
    }

    void on_error(const std::string &message) override {
      _sink.on_error(message);
      delete this;
    }

    void on_complete() override {
      _source_sub->current_result_complete();
      delete this;
    }
  };

  void subscribe(publisher<S> &source, subscriber<value_t> &sink) {
    source->subscribe(*(new source_sub(sink, _fn)));
  }

 private:
  Fn _fn;
  subscription *_source_sub{nullptr};
  publisher<T> _current_sequence;
  subscriber<value_t> *_sink;
};

template <typename S, typename T, typename Op>
struct op_publisher : public publisher_impl<T> {
  using value_t = T;

  op_publisher(publisher<S> &&source, Op &&op)
      : _source(std::move(source)), _op(std::move(op)) {}

  void subscribe(subscriber<value_t> &sink) override {
    _op.subscribe(_source, sink);
  }

  publisher<S> _source;
  Op _op;
};

template <typename T>
struct function_traits : public function_traits<decltype(&T::operator())> {};

template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...) const> {
  enum { arity = sizeof...(Args) };
  typedef ReturnType result_type;

  template <size_t i>
  struct arg {
    typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
  };
};

template <typename T>
struct strip_publisher {};

template <typename T>
struct strip_publisher<publisher<T>> {
  using type = T;
};

}  // namespace impl

template <typename T>
publisher<T> publishers<T>::of(std::initializer_list<T> values) {
  return impl::of_impl(values);
}

template <typename T>
publisher<T> publishers<T>::range(T from, T to) {
  return impl::range_impl(from, to);
}

template <typename T>
publisher<T> publishers<T>::empty() {
  return publisher<T>(new impl::empty_publisher<T>());
}

template <typename T>
publisher<T> publishers<T>::async(
    std::function<void(observer<T> &observer)> init_fn) {
  return publisher<T>(new impl::async_publisher_impl<T>(init_fn));
}

template <typename T>
template <typename State>
publisher<T> publishers<T>::generate(generator<T, State> gen) {
  return publisher<T>(new impl::generator_publisher<T, State>(gen));
}

template <typename T, typename Op>
publisher<typename Op::value_t> operator|(publisher<T> &&src, Op &&op) {
  using value_t = typename Op::value_t;
  auto impl =
      new impl::op_publisher<T, value_t, Op>(std::move(src), std::move(op));
  return publisher<value_t>(impl);
};

template <typename Fn>
auto flat_map(Fn &&fn) {
  using S =
      std::decay_t<typename impl::function_traits<Fn>::template arg<0>::type>;
  using Tx = typename impl::function_traits<Fn>::result_type;
  using T = typename impl::strip_publisher<Tx>::type;
  return impl::flat_map_op<S, T, Fn>{std::move(fn)};
};

template <typename Fn>
auto map(Fn &&fn) {
  using S =
      std::decay_t<typename impl::function_traits<Fn>::template arg<0>::type>;
  using T = typename impl::function_traits<Fn>::result_type;
  return impl::map_op<S, T, Fn>{std::move(fn)};
};

}  // namespace streams