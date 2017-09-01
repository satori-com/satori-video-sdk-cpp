#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "type_traits.h"

namespace streams {

template <typename T>
template <typename OnNext, typename OnComplete, typename OnError>
void publisher_impl<T>::process(OnNext &&on_next, OnComplete &&on_complete,
                                OnError &&on_error) {
  struct sub : subscriber<T> {
    OnNext _on_next;
    OnComplete _on_complete;
    OnError _on_error;
    subscription *_source{nullptr};

    sub(OnNext &&on_next, OnComplete &&on_complete, OnError &&on_error)
        : _on_next(std::move(on_next)),
          _on_complete(std::move(on_complete)),
          _on_error(std::move(on_error)) {}

    void on_next(T &&t) override {
      _on_next(std::move(t));
      _source->request(1);
    }

    void on_complete() override {
      _on_complete();
      delete this;
    }

    void on_error(const std::string &message) override {
      _on_error(message);
      delete this;
    }

    void on_subscribe(subscription &s) override {
      BOOST_ASSERT(!_source);
      _source = &s;
      _source->request(1);
    }
  };

  subscribe(*(new sub{std::move(on_next), std::move(on_complete), std::move(on_error)}));
};

namespace impl {

template <typename T>
struct strip_publisher {};

template <typename T>
struct strip_publisher<publisher<T>> {
  using type = T;
};

template <typename T>
struct async_publisher_impl : public publisher_impl<T> {
  using init_fn_t = std::function<void(observer<T> &observer)>;

  explicit async_publisher_impl(init_fn_t init_fn) : _init_fn(init_fn) {}

  struct sub : subscription, public observer<T> {
    subscriber<T> &_sink;
    std::atomic<long> outstanding;

    explicit sub(subscriber<T> &sink) : _sink(sink) {}

    void request(int n) override { outstanding += n; }

    void cancel() override { BOOST_ASSERT_MSG(false, "not implemented"); }

    void on_next(T &&t) override {
      if (outstanding.fetch_sub(1) <= 0) {
        outstanding++;
        return;
      }

      _sink.on_next(std::move(t));
    }

    void on_error(const std::string &message) override {
      BOOST_ASSERT_MSG(false, "not implemented");
    }

    void on_complete() override { BOOST_ASSERT_MSG(false, "not implemented"); }
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
publisher<T> of_impl(std::vector<T> &&values) {
  struct state {
    std::vector<T> data;
    size_t idx{0};
  };

  return publishers<T>::generate(
      [data = std::move(values)]() mutable { return new state{std::move(data)}; },
      [](state *s, long n, observer<T> &sink) {
        for (long i = 0; i < n && s->idx < s->data.size(); ++i, ++s->idx) {
          sink.on_next(std::move(s->data[s->idx]));
        }

        if (s->idx == s->data.size()) {
          sink.on_complete();
        }
      });
}

template <typename T>
publisher<T> range_impl(T from, T to) {
  return publishers<T>::generate([from]() { return new T{from}; },
                                 [to](T *t, long n, observer<T> &sink) {
                                   for (int i = 0; i < n && *t < to; ++i, ++*t) {
                                     sink.on_next(std::move(*t));
                                   }

                                   if (*t == to) {
                                     sink.on_complete();
                                   }
                                 });
}

struct interval_op {};

inline auto interval(std::chrono::milliseconds i) { return interval_op(); }

template <typename CreateFn, typename GenFn>
struct generator_impl {
  generator_impl(CreateFn &&create_fn, GenFn &&gen_fn)
      : _create_fn(std::move(create_fn)), _gen_fn(std::move(gen_fn)) {}
  generator_impl(const generator_impl &) = delete;
  generator_impl(generator_impl &&) = default;

  CreateFn _create_fn;
  GenFn _gen_fn;
};

template <typename T, typename State, typename Generator>
struct generator_publisher : public publisher_impl<T> {
  using value_t = T;

  struct sub : public subscription, observer<T> {
    Generator _gen;
    subscriber<T> &_sink;

    std::unique_ptr<State> _state;
    bool _active{true};
    bool _in_drain{false};
    long _outstanding{0};

    explicit sub(Generator &&gen, subscriber<T> &sink)
        : _gen(std::move(gen)), _sink(sink), _state(_gen._create_fn()) {}

    void request(int n) override {
      BOOST_ASSERT_MSG(_active, "generator is finished");

      _outstanding += n;
      drain();
    }

    void drain() {
      if (_in_drain) {
        // this is recursive call
        return;
      }

      _in_drain = true;
      while (_active && _outstanding > 0) {
        _gen._gen_fn(_state.get(), _outstanding, *this);
      }
      _in_drain = false;

      if (!_active) {
        delete this;
      }
    }

    void cancel() override {
      _active = false;
      if (!_in_drain) {
        delete this;
      }
    }

    void on_next(T &&t) override {
      _outstanding--;
      _sink.on_next(std::move(t));
    }
    void on_error(const std::string &message) override {
      _sink.on_error(message);
      _active = false;
      if (!_in_drain) {
        delete this;
      }
    }

    void on_complete() override {
      _sink.on_complete();
      _active = false;
      if (!_in_drain) {
        delete this;
      }
    }
  };

  explicit generator_publisher(Generator &&gen) : _gen(std::move(gen)) {}

  void subscribe(subscriber<value_t> &s) override {
    BOOST_ASSERT_MSG(!_subscribed, "single subscription only");
    _subscribed = true;
    s.on_subscribe(*(new sub(std::move(_gen), s)));
  }

  Generator _gen;
  bool _subscribed{false};
};

// operators -------

template <typename Fn>
struct map_op {
  template <typename S>
  struct instance : public subscriber<S>, private subscription {
    using T = typename function_traits<Fn>::result_type;
    using value_t = T;

    static void subscribe(map_op<Fn> &&map_op, publisher<S> &source,
                          subscriber<value_t> &sink) {
      source->subscribe(*(new instance(std::move(map_op._fn), sink)));
    }

    Fn _fn;
    subscriber<T> &_sink;
    subscription *_source{nullptr};

    instance(Fn &&fn, subscriber<T> &sink) : _fn(std::move(fn)), _sink(sink) {}

    void on_next(S &&t) override { _sink.on_next(_fn(std::move(t))); }

    void on_error(const std::string &message) override {
      _sink.on_error(message);
      delete this;
    }

    void on_complete() override {
      _sink.on_complete();
      delete this;
    }

    void on_subscribe(subscription &s) override {
      BOOST_ASSERT(!_source);
      _source = &s;
      _sink.on_subscribe(*this);
    }

    void request(int n) override { _source->request(n); }

    void cancel() override {
      _source->cancel();
      delete this;
    }
  };

  explicit map_op(Fn &&fn) : _fn(fn) {}

  Fn _fn;
};

template <typename Fn>
struct flat_map_op {
  template <typename S>
  struct instance : public subscriber<S>, subscription {
    using Tx = typename function_traits<Fn>::result_type;
    using T = typename impl::strip_publisher<Tx>::type;
    using value_t = T;

    static void subscribe(flat_map_op<Fn> &&op, publisher<S> &source,
                          subscriber<value_t> &sink) {
      source->subscribe(*(new instance(sink, std::move(op._fn))));
    }

    struct fwd_sub;

    subscriber<value_t> &_sink;
    Fn _fn;

    subscription *_source{nullptr};
    bool _in_drain{false};
    bool _active{true};
    bool _source_complete{false};
    bool _requested_next{false};
    std::atomic<long> _outstanding{0};
    fwd_sub *_fwd_sub{nullptr};

    instance(subscriber<value_t> &sink, Fn fn) : _sink(sink), _fn(fn) {}

    void on_subscribe(subscription &s) override {
      BOOST_ASSERT(!_source);
      _source = &s;
      _sink.on_subscribe(*this);
    }

    void on_next(S &&t) override {
      BOOST_ASSERT(!_fwd_sub);
      _requested_next = false;
      _fn(std::move(t))->subscribe(*(_fwd_sub = new fwd_sub(_sink, this)));
      drain();
    }

    void on_error(const std::string &message) override {
      _active = false;
      _sink.on_error(message);
      delete this;
    }

    void on_complete() override {
      _source_complete = true;

      if (!_fwd_sub) {
        _active = false;
        _sink.on_complete();
        if (!_in_drain) {
          delete this;
        }
      } else {
        drain();
      }
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
      while (_active && _outstanding) {
        if (!_fwd_sub) {
          if (_source_complete) {
            on_complete();
            break;
          }

          _requested_next = true;
          _source->request(1);
          if (!_fwd_sub && _requested_next) {
            // next item hasn't arrived yet.
            break;
          }
        } else {
          BOOST_ASSERT(_fwd_sub);
          _fwd_sub->request(_outstanding);
        }
      }
      _in_drain = false;

      if (!_active) {
        delete this;
      }
    }

    void cancel() override {
      _source->cancel();
      if (_fwd_sub) _fwd_sub->cancel();
      delete this;
    }

    void current_result_complete() {
      _fwd_sub = nullptr;
      drain();
    }

    struct fwd_sub : public subscriber<T>, public subscription {
      subscriber<value_t> &_sink;
      instance *_instance;
      subscription *_source{nullptr};

      explicit fwd_sub(subscriber<value_t> &sink, instance *i)
          : _sink(sink), _instance(i) {}

      void on_subscribe(subscription &s) override {
        BOOST_ASSERT(!_source);
        _source = &s;
      }

      void on_next(T &&t) override {
        _instance->_outstanding--;
        _sink.on_next(std::move(t));
      }

      void on_error(const std::string &message) override {
        _sink.on_error(message);
        delete this;
      }

      void on_complete() override {
        _instance->current_result_complete();
        delete this;
      }

      void request(int n) override { _source->request(n); }

      void cancel() override { _source->cancel(); }
    };
  };

  explicit flat_map_op(Fn &&fn) : _fn(std::move(fn)) {}

 private:
  Fn _fn;
};

template <typename S, typename T, typename Op>
struct op_publisher : public publisher_impl<T> {
  using value_t = T;

  op_publisher(publisher<S> &&source, Op &&op)
      : _source(std::move(source)), _op(std::move(op)) {}

  void subscribe(subscriber<value_t> &sink) override {
    using instance_t = typename Op::template instance<S>;
    instance_t::subscribe(std::move(_op), _source, sink);
  }

  publisher<S> _source;
  Op _op;
};

struct take_op {
  explicit take_op(int count) : _n(count) {}

  template <typename S>
  struct instance : subscriber<S>, subscription {
    using value_t = S;

    instance(int n, subscriber<value_t> &sink) : _remaining(n), _sink(sink) {}

    static void subscribe(take_op &&op, publisher<S> &source, subscriber<value_t> &sink) {
      source->subscribe(*(new instance(op._n, sink)));
    }

    void on_next(S &&s) override {
      _sink.on_next(std::move(s));
      _remaining--;
      _outstanding--;

      if (!_remaining) {
        _source_sub->cancel();
        on_complete();
      }
    };

    void on_error(const std::string &message) override {
      _sink.on_error(message);
      delete this;
    };

    void on_complete() override {
      _sink.on_complete();
      delete this;
    };

    void on_subscribe(subscription &s) override {
      _source_sub = &s;
      _sink.on_subscribe(*this);
    };

    void request(int n) override {
      long actual = std::min((long)n, _remaining - _outstanding);
      _outstanding += actual;
      _source_sub->request(actual);
    }

    void cancel() override { _source_sub->cancel(); }

    std::atomic<long> _remaining;
    std::atomic<long> _outstanding{0};
    subscriber<value_t> &_sink;
    subscription *_source_sub;
  };

 private:
  int _n;
};

}  // namespace impl

template <typename T>
publisher<T> publishers<T>::of(std::initializer_list<T> values) {
  return of(std::vector<T>{values});
}

template <typename T>
publisher<T> publishers<T>::of(std::vector<T> &&values) {
  return impl::of_impl(std::move(values));
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
publisher<T> publishers<T>::async(std::function<void(observer<T> &observer)> init_fn) {
  return publisher<T>(new impl::async_publisher_impl<T>(init_fn));
}

template <typename T>
template <typename CreateFn, typename GenFn>
publisher<T> publishers<T>::generate(CreateFn &&create_fn, GenFn &&gen_fn) {
  using generator_t = impl::generator_impl<CreateFn, GenFn>;
  using state_t = std::remove_pointer_t<typename function_traits<CreateFn>::result_type>;

  return publisher<T>(new impl::generator_publisher<T, state_t, generator_t>(
      generator_t(std::move(create_fn), std::move(gen_fn))));
}

template <typename T>
publisher<T> publishers<T>::merge(std::vector<publisher<T>> &&publishers) {
  auto p = streams::publishers<publisher<T>>::of(std::move(publishers));
  return std::move(p) >> flat_map([](publisher<T> &&p) { return std::move(p); });
}

template <typename T, typename Op>
auto operator>>(publisher<T> &&src, Op &&op) {
  using instance_t = typename Op::template instance<T>;
  using value_t = typename instance_t::value_t;
  auto impl = new impl::op_publisher<T, value_t, Op>(std::move(src), std::move(op));
  return publisher<value_t>(impl);
};

template <typename Fn>
auto flat_map(Fn &&fn) {
  return impl::flat_map_op<Fn>{std::move(fn)};
};

template <typename Fn>
auto map(Fn &&fn) {
  return impl::map_op<Fn>{std::move(fn)};
};

inline auto take(int count) { return impl::take_op{count}; }

inline auto head() { return take(1); }

}  // namespace streams