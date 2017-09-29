#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <gsl/gsl>
#include <iostream>
#include <string>
#include <vector>

#include "logging.h"
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

    void on_error(std::error_condition ec) override {
      _on_error(ec);
      delete this;
    }

    void on_subscribe(subscription &s) override {
      CHECK(!_source);
      _source = &s;
      _source->request(1);
    }
  };

  subscribe(*(new sub{std::move(on_next), std::move(on_complete), std::move(on_error)}));
}

namespace impl {

template <typename T>
struct strip_publisher {};

template <typename T>
struct strip_publisher<publisher<T>> {
  using type = T;
};

// special type of source that needs to be pull-drained.
template <typename T>
struct drain_source_impl : public subscription {
  drain_source_impl(streams::subscriber<T> &sink) : _sink(sink) {}

  long needs() const { return _requested - _delivered; }
  long requested() const { return _requested; }
  long delivered() const { return _delivered; }

 protected:
  // return false if you need to break drain loop b/c you are waiting for async event.
  virtual bool drain_impl() = 0;

  void drain() {
    if (_in_drain.load()) {
      _drain_requested = true;
      return;
    }

    _in_drain = true;
    LOG(5) << this << " -> drain";
    auto exit_drain = gsl::finally([this]() {
      LOG(5) << this << "<- drain needs=" << needs() << " _die=" << _die;
      if (_die) {
        delete this;
      } else {
        _in_drain = false;
      }
    });

    while (needs() > 0 && !_die) {
      _drain_requested = false;
      if (!drain_impl()) {
        LOG(5) << this << " drain_impl returned false";
        if (!_drain_requested) {
          LOG(5) << this << " breaking drain";
          break;
        }
        LOG(5) << this << " drain_requested, doing another round";
      }
    }
  }

  void deliver_on_subscribe() {
    LOG(5) << this << " deliver_on_subscribe";
    _sink.on_subscribe(*this);
  }

  void deliver_on_next(T &&t) {
    LOG(5) << this << " deliver_on_next";
    _delivered++;
    CHECK(_delivered <= _requested);
    _sink.on_next(std::move(t));
  }

  void deliver_on_error(std::error_condition ec) {
    LOG(5) << this << " deliver_on_error";
    _sink.on_error(ec);
    if (!_in_drain) {
      delete this;
    } else {
      _die = true;
    }
  }

  void deliver_on_complete() {
    LOG(5) << this << " deliver_on_complete";
    _sink.on_complete();
    if (!_in_drain) {
      delete this;
    } else {
      _die = true;
    }
  }

 private:
  void request(int n) override final {
    LOG(4) << this << " request " << n;
    _requested += n;
    drain();
  }

  void cancel() override final {
    LOG(4) << this << " cancel";
    if (!_in_drain) {
      delete this;
    } else {
      _die = true;
    }
  }

  streams::subscriber<T> &_sink;
  std::atomic<bool> _in_drain{false};
  std::atomic<long> _requested{0};
  std::atomic<long> _delivered{0};
  std::atomic<bool> _die{false};
  std::atomic<bool> _drain_requested{false};
};

template <typename T, typename State>
struct async_publisher_impl : public publisher_impl<T> {
  using init_fn_t = std::function<State *(observer<T> &observer)>;
  using cancel_fn_t = std::function<void(State *)>;

  explicit async_publisher_impl(init_fn_t init_fn, cancel_fn_t cancel_fn)
      : _init_fn(init_fn), _cancel_fn(cancel_fn) {}

  struct sub : subscription, public observer<T> {
    subscriber<T> &_sink;
    std::atomic<long> _requested{0};
    init_fn_t _init_fn;
    cancel_fn_t _cancel_fn;
    State *_state{nullptr};

    explicit sub(subscriber<T> &sink, init_fn_t init_fn, cancel_fn_t cancel_fn)
        : _sink(sink), _init_fn(init_fn), _cancel_fn(cancel_fn) {}

    void init() { _state = _init_fn(*this); }

    void request(int n) override { _requested += n; }

    void cancel() override {
      LOG(5) << "async_publisher_impl::sub::cancel";
      _cancel_fn(_state);
      _state = nullptr;
    }

    void on_next(T &&t) override {
      LOG(5) << "async_publisher_impl::sub::on_next";
      if (_requested.fetch_sub(1) <= 0) {
        LOG(1) << "dropping frame from async_publisher";
        _requested++;
        return;
      }

      _sink.on_next(std::move(t));
    }

    void on_error(std::error_condition err) override {
      _sink.on_error(err);
      delete this;
    }

    void on_complete() override {
      _sink.on_complete();
      delete this;
    }
  };

  virtual void subscribe(subscriber<T> &s) {
    LOG(5) << "async_publisher_impl::subscribe";
    auto inst = new sub(s, _init_fn, _cancel_fn);
    s.on_subscribe(*inst);
    inst->init();
  }

  init_fn_t _init_fn;
  cancel_fn_t _cancel_fn;
};

template <typename T>
struct empty_publisher : public publisher_impl<T>, subscription {
  void subscribe(subscriber<T> &s) override {
    s.on_subscribe(*this);
    s.on_complete();
  }

  void request(int n) override {}
  void cancel() override{};
};

template <typename T>
struct error_publisher : public publisher_impl<T>, subscription {
  explicit error_publisher(std::error_condition ec) : _ec(ec) {}

  void subscribe(subscriber<T> &s) override {
    s.on_subscribe(*this);
    s.on_error(_ec);
  }

  void request(int n) override {}
  void cancel() override{};

  const std::error_condition _ec;
};

template <typename T>
publisher<T> of_impl(std::vector<T> &&values) {
  struct state {
    std::vector<T> data;
    size_t idx{0};
  };

  return generators<T>::stateful(
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
  return generators<T>::stateful([from]() { return new T{from}; },
                                 [to](T *t, long n, observer<T> &sink) {
                                   for (int i = 0; i < n && *t < to; ++i, ++*t) {
                                     sink.on_next(std::move(*t));
                                   }

                                   if (*t == to) {
                                     sink.on_complete();
                                   }
                                 });
}

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

  struct sub : public drain_source_impl<T>, observer<T> {
    Generator _gen;
    std::unique_ptr<State> _state;

    explicit sub(Generator &&gen, subscriber<T> &sink)
        : drain_source_impl<T>(sink), _gen(std::move(gen)), _state(_gen._create_fn()) {}

    ~sub() { LOG(5) << "generator(" << this << ")::~generator"; }

    bool drain_impl() override {
      _gen._gen_fn(_state.get(), drain_source_impl<T>::needs(), *this);
      return false;
    }

    void on_next(T &&t) override { drain_source_impl<T>::deliver_on_next(std::move(t)); }

    void on_error(std::error_condition ec) override {
      LOG(5) << "generator(" << this << ")::on_error";
      drain_source_impl<T>::deliver_on_error(ec);
    }

    void on_complete() override {
      LOG(5) << "generator(" << this << ")::on_complete";
      drain_source_impl<T>::deliver_on_complete();
    }
  };

  explicit generator_publisher(Generator &&gen) : _gen(std::move(gen)) {}

  void subscribe(subscriber<value_t> &s) override {
    CHECK(!_subscribed) << "single subscription only";
    _subscribed = true;
    s.on_subscribe(*(new sub(std::move(_gen), s)));
  }

  Generator _gen;
  bool _subscribed{false};
};

// operators -------

template <typename S, typename T, typename Op>
struct op_publisher : public publisher_impl<T> {
  using value_t = T;

  op_publisher(publisher<S> &&source, Op &&op)
      : _source(std::move(source)), _op(std::move(op)) {}

  void subscribe(subscriber<value_t> &sink) override {
    using instance_t = typename Op::template instance<S>;
    instance_t *instance = new instance_t(std::move(_op), sink);
    _source->subscribe(*instance);
  }

  publisher<S> _source;
  Op _op;
};

template <typename Fn>
struct map_op {
  using T = typename function_traits<std::decay_t<Fn>>::result_type;

  template <typename S>
  struct instance : public subscriber<S>, private subscription {
    using value_t = T;

    static publisher<value_t> apply(publisher<S> &&source, map_op<Fn> &&op) {
      return publisher<value_t>(
          new op_publisher<S, T, map_op<Fn>>(std::move(source), std::move(op)));
    }

    Fn _fn;
    subscriber<T> &_sink;
    subscription *_source{nullptr};

    instance(map_op<Fn> &&op, subscriber<T> &sink)
        : _fn(std::move(op._fn)), _sink(sink) {}

    void on_next(S &&t) override { _sink.on_next(_fn(std::move(t))); }

    void on_error(std::error_condition ec) override {
      _sink.on_error(ec);
      delete this;
    }

    void on_complete() override {
      _sink.on_complete();
      delete this;
    }

    void on_subscribe(subscription &s) override {
      CHECK(!_source);
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
  using Tx = typename function_traits<Fn>::result_type;
  using T = typename impl::strip_publisher<Tx>::type;

  explicit flat_map_op(Fn &&fn) : _fn(std::move(fn)) {}

  template <typename S>
  struct instance : drain_source_impl<T>, subscriber<S> {
    using value_t = T;

    static publisher<value_t> apply(publisher<S> &&source, flat_map_op<Fn> &&op) {
      return publisher<value_t>(
          new op_publisher<S, T, flat_map_op<Fn>>(std::move(source), std::move(op)));
    }

    struct fwd_sub;

    Fn _fn;
    subscription *_source{nullptr};
    bool _active{true};
    bool _source_complete{false};
    bool _requested_next{false};
    fwd_sub *_fwd_sub{nullptr};

    instance(flat_map_op<Fn> &&op, subscriber<value_t> &sink)
        : drain_source_impl<T>(sink), _fn(std::move(op._fn)) {}

    ~instance() {
      LOG(5) << "flat_map_op(" << this << ")::~flat_map_op";
      if (_fwd_sub) {
        _fwd_sub->cancel();
      }
      if (_source) {
        _source->cancel();
      }
    }

    void on_subscribe(subscription &s) override {
      CHECK(!_source);
      _source = &s;
      drain_source_impl<T>::deliver_on_subscribe();
    }

    void on_next(S &&t) override {
      LOG(5) << "flat_map_op(" << this
             << ")::on_next needs=" << drain_source_impl<T>::needs();
      CHECK(!_fwd_sub);
      _requested_next = false;
      _fwd_sub = new fwd_sub(this);
      _fn(std::move(t))->subscribe(*_fwd_sub);
    }

    void on_error(std::error_condition ec) override {
      LOG(5) << "flat_map_op(" << this << ")::on_error";
      _source = nullptr;
      drain_source_impl<T>::deliver_on_error(ec);
    }

    void on_complete() override {
      LOG(5) << "flat_map_op(" << this << ")::on_complete _fwd_sub=" << _fwd_sub;
      CHECK(_source);
      _source_complete = true;
      _source = nullptr;

      if (!_fwd_sub) {
        drain_source_impl<T>::deliver_on_complete();
      } else {
        drain_source_impl<T>::drain();
      }
    }

    bool drain_impl() override {
      LOG(5) << "flat_map_op(" << this << ")::drain_impl fwd_sub=" << _fwd_sub
             << " needs=" << drain_source_impl<T>::needs()
             << "_requested_next=" << _requested_next
             << " _source_complete=" << _source_complete;

      if (_fwd_sub) {
        _fwd_sub->request(drain_source_impl<T>::needs());
        return false;
      }

      if (_requested_next) {
        // next publisher hasn't arrived yet.
        return false;
      }

      if (_source_complete) {
        drain_source_impl<T>::deliver_on_complete();
        return false;
      }

      LOG(5) << "flat_map_op(" << this << ")::drain >requested_next from " << _source;
      _requested_next = true;
      _source->request(1);
      LOG(5) << "flat_map_op(" << this << ")::drain <requested_next";
      return true;
    }

    void current_result_complete() {
      LOG(5) << "flat_map_op(" << this
             << ")::current_result_complete needs=" << drain_source_impl<T>::needs();
      _fwd_sub = nullptr;
      drain_source_impl<T>::drain();
    }

    void current_result_error(std::error_condition ec) {
      LOG(5) << "flat_map_op(" << this << ")::current_result_error";
      _fwd_sub = nullptr;
      drain_source_impl<T>::deliver_on_error(ec);
    }

    struct fwd_sub : public subscriber<T>, public subscription {
      instance *_instance;
      subscription *_source{nullptr};

      explicit fwd_sub(instance *i) : _instance(i) {}

      void on_subscribe(subscription &s) override {
        CHECK(!_source);
        _source = &s;
        _source->request(_instance->needs());
      }

      void on_next(T &&t) override {
        LOG(5) << "flat_map_op(" << _instance << ")::fwd_sub::on_next";
        _instance->deliver_on_next(std::move(t));
      }

      void on_error(std::error_condition ec) override {
        _instance->current_result_error(ec);
      }

      void on_complete() override {
        LOG(5) << "flat_map_op(" << _instance << ")::fwd_sub::on_complete";
        _instance->current_result_complete();
        delete this;
      }

      void request(int n) override {
        LOG(5) << "flat_map_op(" << _instance << ")::fwd_sub::request " << n;
        _source->request(n);
      }

      void cancel() override {
        if (_source) {
          _source->cancel();
        }
        delete this;
      }
    };
  };

 private:
  Fn _fn;
};

template <typename Predicate>
struct take_while_op {
  take_while_op(Predicate &&p) : _p(p) {}

  template <typename T>
  struct instance : public subscriber<T>, private subscription {
    static publisher<T> apply(publisher<T> &&source, take_while_op<Predicate> &&op) {
      return publisher<T>(new op_publisher<T, T, take_while_op<Predicate>>(
          std::move(source), std::move(op)));
    }

    Predicate _p;
    subscriber<T> &_sink;
    subscription *_source{nullptr};

    instance(take_while_op<Predicate> &&op, subscriber<T> &sink)
        : _p(std::move(op._p)), _sink(sink) {}

    void on_next(T &&t) override {
      LOG(5) << "take_while::on_next";
      if (!_p(t)) {
        _source->cancel();
        on_complete();
      } else {
        _sink.on_next(std::move(t));
      }
    }

    void on_error(std::error_condition ec) override {
      _sink.on_error(ec);
      delete this;
    }

    void on_complete() override {
      _sink.on_complete();
      delete this;
    }

    void on_subscribe(subscription &s) override {
      CHECK(!_source);
      _source = &s;
      _sink.on_subscribe(*this);
    }

    void request(int n) override { _source->request(n); }

    void cancel() override {
      _source->cancel();
      delete this;
    }
  };

  Predicate _p;
};

struct take_op {
  explicit take_op(int count) : _n(count) {}

  template <typename S>
  struct instance : subscriber<S>, subscription {
    using value_t = S;
    instance(take_op &&op, subscriber<value_t> &sink) : _n(op._n), _sink(sink) {}

    static publisher<value_t> apply(publisher<S> &&source, take_op &&op) {
      return publisher<value_t>(
          new op_publisher<S, S, take_op>(std::move(source), std::move(op)));
    }

    void on_next(S &&s) override {
      _sink.on_next(std::move(s));
      _received++;

      if (_received == _n) {
        LOG(5) << "take_op(" << this << ") got enough";
        _source_sub->cancel();
        _source_sub = nullptr;
        on_complete();
      }
    };

    void on_error(std::error_condition ec) override {
      _sink.on_error(ec);
      delete this;
    };

    void on_complete() override {
      LOG(5) << "take_op(" << this << ") on_complete _requested=" << _requested
             << " _received=" << _received;
      _sink.on_complete();
      delete this;
    };

    void on_subscribe(subscription &s) override {
      _source_sub = &s;
      _sink.on_subscribe(*this);
    };

    void request(int n) override {
      long actual = std::min((long)n, _n - _requested);
      if (actual > 0) {
        _requested += actual;
        _source_sub->request(actual);
      }
    }

    void cancel() override {
      _source_sub->cancel();
      delete this;
    }

    const int _n;
    std::atomic<long> _received{0};
    std::atomic<long> _requested{0};
    subscriber<value_t> &_sink;
    subscription *_source_sub;
  };

 private:
  int _n;
};

template <typename S, typename T>
struct lift_op {
  explicit lift_op(op<S, T> fn) : _fn(fn) {}

  template <typename S1>
  struct instance {
    static_assert(std::is_same<S, S1>::value, "types do not match");
    using value_t = T;

    static publisher<value_t> apply(publisher<S> &&source, lift_op &&op) {
      return op._fn(std::move(source));
    }
  };

  op<S, T> _fn;
};

template <typename Fn>
struct do_finally_op {
  explicit do_finally_op(Fn &&fn) : _fn(std::move(fn)) {}

  template <typename T>
  struct instance : subscriber<T>, subscription {
    using value_t = T;

    static publisher<T> apply(publisher<T> &&source, do_finally_op<Fn> &&op) {
      return publisher<T>(
          new op_publisher<T, T, do_finally_op<Fn>>(std::move(source), std::move(op)));
    }

    instance(do_finally_op<Fn> &&op, subscriber<T> &sink)
        : _fn(std::move(op._fn)), _sink(sink) {}

    void on_subscribe(subscription &s) override {
      CHECK(!_source_sub);
      _source_sub = &s;
      _sink.on_subscribe(*this);
    }

    void on_next(T &&s) override {
      CHECK(_source_sub);
      _sink.on_next(std::move(s));
    };

    void on_error(std::error_condition ec) override {
      CHECK(_source_sub);
      _source_sub = nullptr;
      _sink.on_error(ec);
      _fn();
      delete this;
    };

    void on_complete() override {
      CHECK(_source_sub);
      LOG(5) << "do_finally(" << this << ")::on_complete";
      _source_sub = nullptr;
      _sink.on_complete();
      _fn();
      delete this;
    };

    void request(int n) override {
      CHECK(_source_sub);
      LOG(5) << "do_finally(" << this << ")::request " << n;
      _source_sub->request(n);
    }

    void cancel() override {
      LOG(5) << "do_finally(" << this << ")::cancel";
      if (_source_sub) {
        _source_sub->cancel();
        _source_sub = nullptr;
      }
      _fn();
      delete this;
    }

    Fn _fn;
    subscriber<value_t> &_sink;
    subscription *_source_sub{nullptr};
  };

  Fn _fn;
};

// piping through a specially implemented operator.
template <typename T, typename Op>
struct pipe_impl {
  static auto apply(publisher<T> &&src, Op &&op) {
    using instance_t = typename Op::template instance<T>;
    return instance_t::apply(std::move(src), std::move(op));
  }
};

// piping through streams::op<T, U>
template <typename T, typename U>
struct pipe_impl<T, op<T, U>> {
  static auto apply(publisher<T> &&src, op<T, U> &&op) {
    return std::move(src) >> impl::lift_op<T, U>(std::move(op));
  }
};

}  // namespace impl

template <typename T>
publisher<T> publishers::of(std::initializer_list<T> values) {
  return of(std::vector<T>{values});
}

template <typename T>
publisher<T> publishers::of(std::vector<T> &&values) {
  return impl::of_impl(std::move(values));
}

template <typename T>
publisher<T> publishers::range(T from, T to) {
  return impl::range_impl(from, to);
}

template <typename T>
publisher<T> publishers::empty() {
  return publisher<T>(new impl::empty_publisher<T>());
}

template <typename T>
publisher<T> publishers::error(std::error_condition ec) {
  return publisher<T>(new impl::error_publisher<T>(ec));
}

template <typename T>
template <typename State>
publisher<T> generators<T>::async(std::function<State *(observer<T> &observer)> init_fn,
                                  std::function<void(State *)> cancel_fn) {
  return publisher<T>(new impl::async_publisher_impl<T, State>(init_fn, cancel_fn));
}

template <typename T>
template <typename CreateFn, typename GenFn>
publisher<T> generators<T>::stateful(CreateFn &&create_fn, GenFn &&gen_fn) {
  using generator_t = impl::generator_impl<CreateFn, GenFn>;
  using state_t = std::remove_pointer_t<typename function_traits<CreateFn>::result_type>;

  return publisher<T>(new impl::generator_publisher<T, state_t, generator_t>(
      generator_t(std::move(create_fn), std::move(gen_fn))));
}

template <typename T>
publisher<T> publishers::merge(std::vector<publisher<T>> &&publishers) {
  auto p = streams::publishers::of(std::move(publishers));
  return std::move(p) >> flat_map([](publisher<T> &&p) { return std::move(p); });
}

template <typename T, typename Op>
auto operator>>(publisher<T> &&src, Op &&op) {
  return impl::pipe_impl<T, Op>::apply(std::move(src), std::move(op));
}

template <typename Fn>
auto flat_map(Fn &&fn) {
  return impl::flat_map_op<Fn>{std::move(fn)};
}

template <typename Fn>
auto map(Fn &&fn) {
  return impl::map_op<Fn>{std::move(fn)};
}

template <typename Predicate>
auto take_while(Predicate &&p) {
  return impl::take_while_op<Predicate>{std::move(p)};
}

inline auto take(int count) { return impl::take_op(count); }

inline auto head() { return take(1); }

template <typename Fn>
auto do_finally(Fn &&fn) {
  return impl::do_finally_op<Fn>{std::move(fn)};
}

}  // namespace streams