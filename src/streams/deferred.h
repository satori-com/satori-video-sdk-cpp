// deferred<T> - synchronous deferred value (aka synchronous future).
//
// deferred<T> is a value that will become available later, once resolved. Its core API
// consists of only two methods:
//     - .on([](t){}) - registers value listener. Can be called only once.
//     - .resolve(t) - calls all registered listeners with a given value. Can be called
//                     only once.
//
// deferred<T> has reference semantics, i.e. when copied, it does not introduce new
// asynchronously available value. Only new value constructors do that.
//
// Usage example:
//
//      deferred<int> sum_async() {
//        deferred<int> result;
//        async_sum_executor.schedule([](int s) {
//              result.resolve(s);
//        });
//        return result;
//      }
//
//      deferred<int> sum = sum_async();
//      sum.on([](const status_or<int> result) {
//         result.verify_ok();
//         std::cout << "sum = " << *result;
//      });
//
// Important part of deferred<T> is error propagation: deferred<T> can be resolved into
// error, using fail method.
//
// Usage:
//
//      result.fail(errors::MyError);
//      result.on(const status_or<int> value) {
//          value.verify_not_ok();
//      });
//
// deferred<T> values can be chained together using deferred<T>.map with a function
// f : T->U.
//
//      deferred<int> i;
//      deferred<std::string> s = i.map([](int x) { return std::to_string(x); });
//      s.on(const status_or<std::string> value) {
//          value.verify_ok();
//          std::cout << *value;
//      });
//      i.resolve(42);  // will print 42;
//
// Two asynchronous operations can be chained together using deferred<T>.then with a
// function f : T->deferred<U>.
//
//      deferred<int> i;
//      deferred<std::string> s = i.then([](int x) {
//          deferred<std::string> new_async_value = start_async_operation(x);
//          return new_async_value;
//      });
//
// deferred<T>.map & deferred<T>.then perform correct error propagation through the chain
// of deferred values. Notice that callbacks in map and then won't be called for errors.
// deferred<T>.on callback is called regardless.

#pragma once

#include <chrono>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "error_or.h"

namespace satori {
namespace video {

namespace streams {
template <typename T>
class deferred;

template <>
class deferred<void>;

template <typename T>
class deferred_impl;

template <>
class deferred_impl<void>;

// type computations necessary for defining interface and implementation.
namespace {

// unwrap_deferred<deferred<T>>::type = T;
template <class T>
struct unwrap_deferred {};
template <class T>
struct unwrap_deferred<deferred<T>> {
  using type = T;
};

// Type of Fn(T)
template <typename Fn /* : T -> X */, typename T>
struct result_type {
  using type = typename std::result_of<Fn(const T &)>::type;
};
template <typename Fn /* : () -> X */>
struct result_type<Fn, void> {
  using type = typename std::result_of<Fn()>::type;
};

// Type traits for deferred<T>.map(Fn);
template <typename Fn, typename T>
struct map_types {
  using result_element_t = typename result_type<Fn, T>::type;
  using return_t = deferred<result_element_t>;
  using impl_return_t = std::shared_ptr<deferred_impl<result_element_t>>;
};

// Type traits for deferred<T>.then(Fn);
template <typename Fn, typename T>
struct then_types {
  using result_element_t =
      typename unwrap_deferred<typename result_type<Fn, T>::type>::type;
  using return_t = deferred<result_element_t>;
  using impl_return_t = std::shared_ptr<deferred_impl<result_element_t>>;
};

template <typename T>
struct impl_types {
  using value_t = error_or<T>;
  using callback_type = std::function<void(value_t &&)>;
  inline static bool ok(const value_t &v) { return v.ok(); }
};
template <>
struct impl_types<void> {
  using value_t = std::error_condition;
  using callback_type = std::function<void(value_t)>;
  inline static bool ok(const value_t &v) { return !(bool)v; }
};
}  // namespace

// base class for deferred<T> without any specialized constructors.
template <typename T>
class deferred_base {
 public:
  using element_t = T;
  // status_or<T> | status if T = void.
  using value_t = typename impl_types<T>::value_t;

  bool resolved() const { return _impl->resolved(); }
  bool ok() const { return _impl->ok(); }

  // Calls f(status_or<T>) when value is available.
  template <class Fn /* value_t -> () */>
  void on(Fn f) {
    _impl->on(f);
  }

  void fail(std::error_condition ec) { _impl->fail(ec); }

  // Resolves the deferred.
  void resolve(value_t &&t) { _impl->resolve(std::move(t)); }

  // Transforms the value once resolved into immediate value.
  template <class Fn /* : T -> U */>
  typename map_types<Fn, T>::return_t map(Fn f) const {
    using U = typename map_types<Fn, T>::result_element_t;
    return deferred<U>(_impl->map(f));
  }

  // Transform the value once resolved into another deferred.
  template <class Fn /* : T -> deferred<U> */>
  typename then_types<Fn, T>::return_t then(Fn f) const {
    using U = typename then_types<Fn, T>::result_element_t;
    return deferred<U>(_impl->then(f));
  }

 protected:
  // deferred is a shared_ptr. This gives it reference semantics.
  std::shared_ptr<deferred_impl<T>> _impl;

  explicit deferred_base(std::shared_ptr<deferred_impl<T>> impl)
      : _impl(std::move(impl)) {}

  template <typename>
  friend class deferred_base;
};

// deferred<T> - synchronous deferred value.
template <typename T>
class deferred : public deferred_base<T> {
 private:
  explicit deferred(std::shared_ptr<deferred_impl<T>> impl) : deferred_base<T>(impl) {}

  template <typename>
  friend class deferred_base;

 public:
  // create unresolved deferred value
  deferred() : deferred_base<T>(std::make_shared<deferred_impl<T>>()) {}

  // create already failed deferred value. status must be not ok.
  deferred(std::error_condition ec)
      : deferred_base<T>(std::make_shared<deferred_impl<T>>(ec)) {}

  // create already resolved deferred value.
  deferred(const T &t) : deferred_base<T>(std::make_shared<deferred_impl<T>>(t)) {}

  // create already resolved deferred value.
  deferred(T &&t) : deferred_base<T>(std::make_shared<deferred_impl<T>>(std::move(t))) {}

  // ignore return value
  operator deferred<void>();
};

// deferred<void> is basically a deferred status.
// deferred<void> is useful when you don't have a value, but still need to communicate
// async success/failure. Class specialization is needed to provide different set of
// constructors since you can't have void&.
template <>
class deferred<void> : public deferred_base<void> {
 private:
  explicit deferred(std::shared_ptr<deferred_impl<void>> impl)
      : deferred_base<void>(std::move(impl)) {}

  template <typename>
  friend class satori::video::streams::
      deferred_base;  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=52625

 public:
  deferred() : deferred_base<void>(std::make_shared<deferred_impl<void>>()) {}

  deferred(std::error_condition ec)
      : deferred_base<void>(std::make_shared<deferred_impl<void>>(ec)) {}

  void resolve() { deferred_base<void>::resolve(std::error_condition{}); }
};

template <typename T>
deferred<T>::operator deferred<void>() {
  return this->map([](const T & /*t*/) {});
}

// ---------- IMPLEMENTATION DETAILS ----------

template <typename T>
class deferred_impl_base {
 protected:
  using value_t = typename impl_types<T>::value_t;

  value_t _value;
  bool _resolved{false};
  bool _has_callback{false};

  typename impl_types<T>::callback_type _resolve_cb;

  deferred_impl_base() : _value(std::error_condition(stream_error::NotInitialized)) {}

  deferred_impl_base(value_t value) : _value(std::move(value)), _resolved(true) {}

 public:
  bool resolved() const { return _resolved; }

  bool ok() const {
    CHECK(resolved());
    return impl_types<T>::ok(_value);
  }

  template <class Fn>
  void on(Fn f) {
    CHECK(!_has_callback);
    _has_callback = true;

    if (!_resolved) {
      _resolve_cb = std::move(f);
    } else {
      f(std::move(_value));
    }
  }

  void fail(std::error_condition ec) {
    CHECK(ec);
    _value = ec;
    mark_resolved_and_notify();
  }

  void resolve(value_t &&t) {
    _value = std::move(t);
    mark_resolved_and_notify();
  }

  template <class Fn>
  inline typename map_types<Fn, T>::impl_return_t map(Fn f);

  template <class Fn>
  inline typename then_types<Fn, T>::impl_return_t then(Fn f);

 private:
  void mark_resolved_and_notify() {
    CHECK(!_resolved);
    _resolved = true;
    if (!_has_callback) {
      return;
    }
    _resolve_cb(std::move(_value));
  }
};

template <typename T>
class deferred_impl : public deferred_impl_base<T> {
  template <typename>
  friend class deferred;

  template <typename>
  friend class deferred_impl;

 public:
  // public for make_shared
  deferred_impl() = default;

  deferred_impl(std::error_condition ec) : deferred_impl_base<T>(ec) { CHECK(ec); }

  deferred_impl(const T &t) : deferred_impl_base<T>(t) {}
};

template <>
class deferred_impl<void> : public deferred_impl_base<void> {
  template <typename>
  friend class deferred;

 public:
  // public for make_shared
  deferred_impl() = default;

  deferred_impl(std::error_condition ec) : deferred_impl_base<void>(ec) {}
};

// because void& doesn't exist, various T->U forwarding chains should be specialized.
namespace {
// Forward error value from t to p. Implies that !t.ok();
template <typename T, typename U>
struct error_fwd {
  inline static void fwd(const error_or<T> &t, std::shared_ptr<deferred_impl<U>> p) {
    t.check_not_ok();
    p->resolve(t.error_condition());
  }
};

template <typename U>
struct error_fwd<void, U> {
  using T = void;

  inline static void fwd(const std::error_condition &ec,
                         std::shared_ptr<deferred_impl<U>> p) {
    CHECK((bool)ec);
    p->resolve(std::error_condition{ec});
  }
};

// Apply f to *t. Implies that t.ok(); Performs conversion of resulting void
// value to status::OK.
template <class Fn /* T -> U */, typename T, typename U>
struct apply_fn {
  inline static U apply(Fn f, const error_or<T> &t) { return f(t.get()); }
};
template <class Fn /* void -> U */, typename U>
struct apply_fn<Fn, void, U> {
  inline static U apply(Fn f, const std::error_condition & /*t*/) { return f(); }
};
template <class Fn /* T -> void */, typename T>
struct apply_fn<Fn, T, std::error_condition> {
  using U = std::error_condition;
  inline static U apply(Fn f, const error_or<T> &t) {
    f(t.get());
    return {};
  }
};
template <class Fn /* void -> void */>
struct apply_fn<Fn, void, std::error_condition> {
  using U = std::error_condition;
  inline static U apply(Fn f, const std::error_condition & /*t*/) {
    f();
    return {};
  }
};
}  // namespace

// deferred_impl_base<T> methods implementation,

template <typename T>
template <class Fn /* : T -> U */>
typename map_types<Fn, T>::impl_return_t deferred_impl_base<T>::map(Fn f) {
  using U = typename map_types<Fn, T>::result_element_t;
  using u_value_t = typename impl_types<U>::value_t;

  auto result = std::make_shared<deferred_impl<U>>();
  on([result, f](const value_t &value) mutable {
    if (impl_types<T>::ok(value)) {
      u_value_t u = apply_fn<Fn, T, u_value_t>::apply(f, value);
      result->resolve(std::move(u));
    } else {
      error_fwd<T, U>::fwd(value, result);
    }
  });
  return result;
}

template <typename T>
template <class Fn /* : T -> U */>
typename then_types<Fn, T>::impl_return_t deferred_impl_base<T>::then(Fn f) {
  using U = typename then_types<Fn, T>::result_element_t;
  using u_value_t = typename impl_types<U>::value_t;

  auto result = std::make_shared<deferred_impl<U>>();
  on([result, f](const value_t &value) mutable {
    if (impl_types<T>::ok(value)) {
      deferred<U> u = apply_fn<Fn, T, deferred<U>>::apply(f, value);
      u.on(
          [result](u_value_t &&u_value) mutable { result->resolve(std::move(u_value)); });
    } else {
      error_fwd<T, U>::fwd(value, result);
    }
  });
  return result;
}

}  // namespace streams
}  // namespace video
}  // namespace satori