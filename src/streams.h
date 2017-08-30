// Reactive Streams (http://www.reactive-streams.org/) implementation.
#pragma once

#include <functional>
#include <initializer_list>
#include <memory>

namespace streams {

struct subscription {
  virtual ~subscription() = default;
  virtual void request(int n) = 0;
  virtual void cancel() = 0;
};

template <typename T>
struct observer {
  virtual ~observer() = default;
  virtual void on_next(const T &t) = 0;
  virtual void on_error(const std::string &message) = 0;
  virtual void on_complete() = 0;
};

template <typename T>
struct subscriber : public observer<T> {
  virtual ~subscriber() = default;
  // subscriber instance should be kept alive until on_error/on_complete or
  // cancel() call.
  virtual void on_subscribe(subscription &s) = 0;
};

template <typename T>
struct publisher_impl {
  using value_t = T;

  virtual ~publisher_impl() = default;
  // subscriber instance should be kept alive until on_error/on_complete or
  // cancel() call.
  virtual void subscribe(subscriber<T> &s) = 0;
};

template <typename T>
using publisher = std::unique_ptr<publisher_impl<T>>;

// process stream with an operator and return the result.
template <typename T, typename Op>
publisher<typename Op::value_t> operator|(publisher<T> &&src, Op &&op);

// ----------------------------------------------------------------------

template <typename T, typename State>
struct generator {
  using create_fn_t = std::function<State *()>;
  using gen_fn_t = std::function<void(State *, int, observer<T> &)>;

  create_fn_t create_fn;
  gen_fn_t gen_fn;
};

template <typename T>
struct publishers {
  static publisher<T> empty();

  template <typename State>
  static publisher<T> generate(generator<T, State> gen);

  static publisher<T> async(std::function<void(observer<T> &observer)> init_fn);

  static publisher<T> of(std::initializer_list<T> values);

  static publisher<T> range(T from, T to);
};

}  // namespace streams

#include "streams_impl.h"