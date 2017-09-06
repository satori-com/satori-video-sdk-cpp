// Reactive Streams (http://www.reactive-streams.org/) implementation.
#pragma once

#include <functional>
#include <initializer_list>
#include <memory>
#include <system_error>

namespace streams {

struct subscription {
  virtual ~subscription() = default;
  virtual void request(int n) = 0;
  virtual void cancel() = 0;
};

template <typename T>
struct observer {
  virtual ~observer() = default;
  virtual void on_next(T &&t) = 0;
  virtual void on_error(std::error_condition ec) = 0;
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

  // subscribe to a publisher using given callbacks.
  template <typename OnNext, typename OnComplete, typename OnError>
  void process(OnNext &&on_next, OnComplete &&on_complete, OnError &&on_error);
};

template <typename T>
using publisher = std::unique_ptr<publisher_impl<T>>;

// process stream with an operator and return the result.
template <typename T, typename Op>
auto operator>>(publisher<T> &&src, Op &&op);

// ----------------------------------------------------------------------
template <typename T>
struct publishers {
  // Creates empty stream.
  static publisher<T> empty();

  // Creates stream in error state.
  static publisher<T> error(std::error_condition ec);

  // Stateful stream generator.
  // create_fn - State*() - creates new state object
  // gen_fn - void(State* state, int n, observer<T>) - called periodically. Should
  // generate no more than n objects. Less is OK.
  template <typename CreateFn, typename GenFn>
  static publisher<T> generate(CreateFn &&create_fn, GenFn &&gen_fn);

  // Creates a stream from extern asynchronous process.
  static publisher<T> async(std::function<void(observer<T> &observer)> init_fn);

  // Stream of given values.
  static publisher<T> of(std::initializer_list<T> values);

  // Stream of given values.
  static publisher<T> of(std::vector<T> &&values);

  // Stream of values [from, to).
  static publisher<T> range(T from, T to);

  // Streams each publisher consequently.
  static publisher<T> merge(std::vector<publisher<T>> &&publishers);
};

// head operation produces a stream with only first element.
auto head();

// take operation produces a stream with only count elements.
auto take(int count);

// map operation transforms each element into immediate value.
template <typename Fn>
auto map(Fn &&fn);

// flat_map operation transforms each element a stream and produces element out of them
// consequently.
template <typename Fn>
auto flat_map(Fn &&fn);

// do_finally operation creates a new stream that calls fn when underlying stream is
// either signals on_complete, on_error or gets cancelled by downstream
template <typename Fn>
auto do_finally(Fn &&fn);

template <typename S, typename T>
using op = std::function<publisher<T>(publisher<S> &&)>;

// lift publisher -> publisher function to an operator.
template <typename S, typename T>
auto lift(op<S, T> fn);

template <typename S, typename T>
auto lift(publisher<T> (*fn)(publisher<S> &&)) {
  return lift(static_cast<op<S, T>>(fn));
};

}  // namespace streams

#include "streams_impl.h"