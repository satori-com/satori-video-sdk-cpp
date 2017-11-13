// Reactive Streams (http://www.reactive-streams.org/) implementation.
#pragma once

#include <functional>
#include <initializer_list>
#include <list>
#include <memory>
#include <queue>
#include <system_error>
#include <vector>

#include "deferred.h"

namespace satori {
namespace video {

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
  ~subscriber() override = default;
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

  template <typename OnNext>
  deferred<void> process(OnNext &&on_next);
};

template <typename T>
using publisher = std::unique_ptr<publisher_impl<T>>;

template <typename S, typename T>
using op = std::function<publisher<T>(publisher<S> &&)>;

// process stream with an operator and return the result.
// Operator can be stream::op or one of the operators defined below.
template <typename T, typename Op>
auto operator>>(publisher<T> &&src, Op &&op);

// ----- Creating streams

struct publishers {
  // Creates empty stream.
  template <typename T>
  static publisher<T> empty();

  // Creates stream in error state.
  template <typename T>
  static publisher<T> error(std::error_condition ec);

  // Stream of given values.
  template <typename T>
  static publisher<T> of(std::initializer_list<T> values);

  template <typename T>
  static publisher<T> of(std::vector<T> &&values);

  template <typename T>
  static publisher<T> of(std::queue<T> &&values);

  template <typename T>
  static publisher<T> of(std::list<T> &&values);

  // Stream of values [from, to).
  template <typename T>
  static publisher<T> range(T from, T to);

  // Streams each publisher consequently.
  template <typename T>
  static publisher<T> concat(std::vector<publisher<T>> &&publishers);

  template <typename T>
  static publisher<T> concat(publisher<T> &&p1, publisher<T> &&p2) {
    std::vector<publisher<T>> publishers;
    publishers.push_back(std::move(p1));
    publishers.push_back(std::move(p2));
    return concat(std::move(publishers));
  }

  // Streams interleaved publishers.
  template <typename T>
  static publisher<T> merge(std::vector<publisher<T>> &&publishers);

  template <typename T>
  static publisher<T> merge(publisher<T> &&p1, publisher<T> &&p2) {
    std::vector<publisher<T>> publishers;
    publishers.push_back(std::move(p1));
    publishers.push_back(std::move(p2));
    return merge(std::move(publishers));
  }
};

template <typename T>
struct generators {
  // Stateful stream generator.
  // create_fn - State*() - creates new state object
  // gen_fn - bool(State* state, observer<T>) - called periodically. Should
  // generate 1 object.
  template <typename CreateFn, typename GenFn>
  static publisher<T> stateful(CreateFn &&create_fn, GenFn &&gen_fn);

  // Creates a stream from external asynchronous process.
  // Since asynchronous process can't cooperate with stream control,
  // its values are accumulated into the queue.
  // start_fn - State*(observer<T>&) - starts the asynchronous process
  // stop_fn - void(State*) - stops the asynchronous proces
  template <typename State, typename StartFn, typename StopFn>
  static publisher<std::queue<T>> async(StartFn &&start_fn, StopFn &&stop_fn);
};

// read file line by line.
publisher<std::string> read_lines(const std::string &filename);

// ----- Transforming streams

// head operation produces a stream with only first element.
auto head();

// take operation produces a stream with only count elements.
auto take(int count);

// take operation produces a stream while predicate is true.
template <typename Predicate>
auto take_while(Predicate &&p);

// map operation transforms each element into immediate value.
template <typename Fn>
auto map(Fn &&fn);

// flat_map operation transforms each element a stream and produces element out of them
// consequently.
template <typename Fn>
auto flat_map(Fn &&fn);

// flatten takes a stream of collection<T> and emits each element separately
auto flatten();

// do_finally operation creates a new stream that calls fn when underlying stream is
// either signals on_complete, on_error or gets cancelled by downstream
template <typename Fn>
auto do_finally(Fn &&fn);

}  // namespace streams
}  // namespace video
}  // namespace satori

#include "streams_impl.h"