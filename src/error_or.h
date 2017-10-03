#pragma once

#include <memory>
#include <utility>

#include "error.h"
#include "logging.h"

namespace rtm {
namespace video {
template <typename T>
class error_or;

namespace impl {
template <typename T>
using static_not = std::integral_constant<bool, !T::value>;

template <typename X>
constexpr bool is_error_condition() {
  using x = typename std::decay<X>::type;
  return std::is_same<std::error_condition, x>::value;
}

template <typename T>
struct is_error_or_helper {
  static constexpr bool value = false;
};

template <typename X>
struct is_error_or_helper<error_or<X>> {
  static constexpr bool value = true;
};

template <typename X>
constexpr bool is_error_or() {
  using x = typename std::decay<X>::type;
  return is_error_or_helper<x>::value;
}
}  // namespace impl

template <typename T>
class error_or {
  // simple test.
  static_assert(!impl::is_error_or<int>(), "test failed");
  static_assert(impl::is_error_or<error_or<int>>(), "test failed");

 public:
  error_or() : _ec{} { ::new (dataptr()) T(); }

  // Universal reference constructor has to be disabled for errors
  // and for error_or itself.
  // For latter: https://akrzemi1.wordpress.com/2013/10/10/too-perfect-forwarding/
  template <typename X,
            typename std::enable_if<!impl::is_error_condition<X>()>::type * = nullptr,
            typename std::enable_if<!impl::is_error_or<X>()>::type * = nullptr>
  error_or(X &&t) : _ec{} {
    ::new (dataptr()) T(std::forward<X>(t));
  }

  error_or(T t) : _ec{} { ::new (dataptr()) T(std::move(t)); }

  error_or(std::error_condition ec) : _ec(ec) { check_not_ok(); }

  error_or(const error_or<T> &other) : _ec(other._ec) {
    if (ok()) ::new (dataptr()) T(other._s.t);
  }

  error_or(error_or<T> &&other) : _ec(other._ec) {
    if (ok()) ::new (dataptr()) T(std::move(other._s.t));
  }

  ~error_or() {
    if (ok()) _s.t.~T();
  }

  error_or<T> &operator=(const error_or<T> &other) {
    _ec = other._ec;
    if (ok()) ::new (dataptr()) T(other._s.t);
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
    _ec = video_error::ValueWasMoved;
    return std::move(_s.t);
  }

  const T &operator*() const { return get(); }

  const std::error_condition &error_condition() const {
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
}  // namespace video
}  // namespace rtm
