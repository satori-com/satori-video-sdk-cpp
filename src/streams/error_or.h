#pragma once

#include <memory>
#include <utility>

#include "../logging.h"
#include "stream_error.h"

namespace satori {
namespace video {
namespace streams {

template <typename T>
class error_or;

namespace impl {

template <typename T>
struct is_error_or_impl {
  static constexpr bool value = false;
};

template <typename X>
struct is_error_or_impl<error_or<X>> {
  static constexpr bool value = true;
};

}  // namespace impl

template <typename X>
constexpr bool is_error_or() {
  using x = typename std::decay<X>::type;
  return impl::is_error_or_impl<x>::value;
}

template <typename T>
class error_or {
 public:
  error_or() { ::new (dataptr()) T(); }

  // Universal reference constructor has to be disabled for errors
  // and for error_or itself.
  // For latter: https://akrzemi1.wordpress.com/2013/10/10/too-perfect-forwarding/
  template <typename X, typename IF1 = std::enable_if_t<!is_error_condition<X>()>,
            typename IF2 = std::enable_if_t<!is_error_or<X>()>>
  error_or(X &&t) {
    ::new (dataptr()) T(std::forward<X>(t));
  }

  error_or(T t) { ::new (dataptr()) T(std::move(t)); }

  error_or(std::error_condition ec) : _ec(ec) { check_not_ok(); }

  error_or(const error_or<T> &other) : _ec(other._ec) {
    if (ok()) {
      ::new (dataptr()) T(other._s.t);
    }
  }

  error_or(error_or<T> &&other) noexcept : _ec(other._ec) {
    if (ok()) {
      ::new (dataptr()) T(std::move(other._s.t));
    }
  }

  ~error_or() {
    if (ok()) {
      _s.t.~T();
    }
  }

  error_or<T> &operator=(const error_or<T> &other) {
    _ec = other._ec;
    if (ok()) {
      ::new (dataptr()) T(other._s.t);
    }
    return *this;
  }

  bool ok() const { return !_ec; }

  void check_ok() const { CHECK(ok()); }

  void check_not_ok() const { CHECK(!ok()); }

  const T &get() const {
    check_ok();
    return _s.t;
  }

  T &&move() {
    check_ok();
    _ec = stream_error::VALUE_WAS_MOVED;
    return std::move(_s.t);
  }

  const T &operator*() const { return get(); }

  std::error_condition error_condition() const {
    check_not_ok();
    return _ec;
  }

  std::string error_message() const { return _ec.message(); }

 private:
  T *dataptr() { return &_s.t; }

  std::error_condition _ec;

  // storage union is needed to control T constructor invocations.
  union storage {
    storage() {}   // NOLINT : = default calls t.T();
    ~storage() {}  // NOLINT : = default calls t.~T();

    uint8_t dummy;
    T t;
  } _s;
};

}  // namespace streams

}  // namespace video
}  // namespace satori