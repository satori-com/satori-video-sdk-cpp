#define BOOST_TEST_MODULE DeferredTest
#include <boost/test/included/unit_test.hpp>

#include "streams/deferred.h"

using namespace rtm::video::streams;

BOOST_AUTO_TEST_CASE(deferred_resolve_test) {
  int value = 0;
  deferred<int> i;
  i.on([&value](error_or<int> i1) { value = *i1; });

  BOOST_TEST(value == 0);
  i.resolve(345);
  BOOST_TEST(value == 345);
}

BOOST_AUTO_TEST_CASE(deferred_resolve_error_test) {
  std::string value;
  deferred<int> i;
  i.on([&value](error_or<int> i1) {
    i1.check_not_ok();
    value = i1.error_message();
  });

  BOOST_TEST(value == "");
  i.resolve(std::error_condition(stream_error::NotInitialized));
  BOOST_TEST(value.find("not initialized") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(deferred_map_test) {
  deferred<int> i;
  deferred<std::string> s = i.map([](int i1) { return std::to_string(i1); });

  std::string value;
  s.on([&value](error_or<std::string> s1) { value = *s1; });

  i.resolve(123);
  BOOST_TEST(value == "123");
}

BOOST_AUTO_TEST_CASE(deferred_map_error_test) {
  deferred<int> i;
  deferred<std::string> s = i.map([](int i1) { return std::to_string(i1); });

  std::string value;
  s.on([&value](error_or<std::string> s1) {
    s1.check_not_ok();
    value = s1.error_message();
  });

  BOOST_TEST(value == "");
  i.resolve(std::error_condition(stream_error::NotInitialized));
  BOOST_TEST(value.find("not initialized") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(deferred_then_test) {
  deferred<bool> sema;
  deferred<int> i;
  deferred<std::string> s = i.then([sema](int i1) mutable {
    deferred<std::string> result;
    sema.on([sema, result, i1](error_or<bool>) mutable {
      result.resolve(std::to_string(i1));
    });
    return result;
  });

  std::string value;
  s.on([&value](error_or<std::string> s1) { value = *s1; });

  i.resolve(123);
  BOOST_TEST(value == "");
  sema.resolve(true);
  BOOST_TEST(value == "123");
}

BOOST_AUTO_TEST_CASE(deferred_then_error_test) {
  deferred<bool> sema;
  deferred<int> i;
  deferred<std::string> s = i.then([sema](int i) mutable {
    deferred<std::string> result;
    sema.on(
        [sema, result, i](error_or<bool>) mutable { result.resolve(std::to_string(i)); });
    return result;
  });

  std::string value;
  s.on([&value](error_or<std::string> s1) {
    s1.check_not_ok();
    value = s1.error_message();
  });

  i.resolve(std::error_condition(stream_error::NotInitialized));
  // then block is not executed when there's error.
  BOOST_TEST(value.find("not initialized") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(deferred_on_after_resolve) {
  deferred<int> i;
  i.resolve(42);

  int value = 0;
  i.on([&value](error_or<int> i1) { value = *i1; });
  BOOST_TEST(value == 42);
}

BOOST_AUTO_TEST_CASE(deferred_already_resolved) {
  deferred<int> i(42);

  int value = 0;
  i.on([&value](error_or<int> i1) { value = *i1; });
  BOOST_TEST(value == 42);
}

BOOST_AUTO_TEST_CASE(deferred_error_resolved) {
  deferred<int> i{std::error_condition(stream_error::NotInitialized)};

  std::string value;
  i.on([&value](error_or<int> i1) {
    i1.check_not_ok();
    value = i1.error_message();
  });
  BOOST_TEST(value.find("not initialized") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(deferred_void_normal_resolve) {
  deferred<void> v;

  bool resolved = false;
  v.on([&resolved](const std::error_condition &s) {
    BOOST_TEST(!s);
    resolved = true;
  });

  v.resolve();
  BOOST_TEST(resolved);
}

BOOST_AUTO_TEST_CASE(deferred_void_error_resolve) {
  deferred<void> v;

  std::string value;
  v.on([&value](const std::error_condition &s) {
    BOOST_TEST((bool)s);
    value = s.message();
  });

  v.fail(std::error_condition(stream_error::NotInitialized));
  BOOST_TEST(value.find("not initialized") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(deferred_map_to_void) {
  deferred<int> i;
  deferred<void> v = i.map([](int) {});

  bool resolved = false;
  v.on([&resolved](const std::error_condition &s) {
    BOOST_TEST(!s);
    resolved = true;
  });

  i.resolve(1);
  BOOST_TEST(resolved);
}

BOOST_AUTO_TEST_CASE(deferred_map_to_void_error) {
  deferred<int> i;
  deferred<void> v = i.map([](int) {});

  std::string value;
  v.on([&value](const std::error_condition &s) {
    BOOST_TEST((bool)s);
    value = s.message();
  });

  i.resolve(std::error_condition(stream_error::NotInitialized));
  BOOST_TEST(value.find("not initialized") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(deferred_then_to_void) {
  deferred<bool> sema;
  deferred<int> i;
  deferred<void> v = i.then([sema](int) mutable {
    deferred<void> result;
    sema.on([result](error_or<bool>) mutable { result.resolve(); });
    return result;
  });

  bool resolved = false;
  v.on([&resolved](const std::error_condition &s) {
    BOOST_TEST(!s);
    resolved = true;
  });

  i.resolve(1);
  BOOST_TEST(!resolved);
}

BOOST_AUTO_TEST_CASE(deferred_map_void) {
  deferred<void> v;
  deferred<int> i = v.map([]() { return 42; });

  int value = 0;
  i.on([&value](const error_or<int> &s) {
    s.check_ok();
    value = *s;
  });

  v.resolve();
  BOOST_TEST(value == 42);
}

BOOST_AUTO_TEST_CASE(deferred_map_void_void) {
  deferred<void> v1;
  deferred<void> v2 = v1.map([]() {});

  bool resolved = false;
  v2.on([&resolved](const std::error_condition &s) {
    BOOST_TEST(!s);
    resolved = true;
  });

  v1.resolve();
  BOOST_TEST(resolved == true);
}

BOOST_AUTO_TEST_CASE(deferred_map_void_error) {
  deferred<void> v;
  deferred<int> i = v.map([]() { return 42; });

  std::string value;
  i.on([&value](const error_or<int> &s) {
    s.check_not_ok();
    value = s.error_message();
  });

  v.fail(stream_error::NotInitialized);
  BOOST_TEST(value.find("not initialized") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(deferred_then_void) {
  deferred<bool> sema;
  deferred<void> v;
  deferred<int> i = v.then([sema]() mutable {
    deferred<int> result;
    sema.on([result](error_or<bool>) mutable { result.resolve(42); });
    return result;
  });

  int value = 0;
  i.on([&value](const error_or<int> &s) {
    s.check_ok();
    value = *s;
  });

  v.resolve();
  BOOST_TEST(value == 0);
  sema.resolve(true);
  BOOST_TEST(value == 42);
}

BOOST_AUTO_TEST_CASE(deferred_then_void_void) {
  deferred<void> sema;
  deferred<void> v1;
  deferred<void> v2 = v1.then([sema]() mutable {
    deferred<void> result;
    sema.on([result](std::error_condition ec) mutable {
      BOOST_TEST(!ec);
      result.resolve();
    });
    return result;
  });

  bool resolved = false;
  v2.on([&resolved](const std::error_condition &s) {
    BOOST_TEST(!s);
    resolved = true;
  });

  v1.resolve();
  BOOST_TEST(!resolved);
  sema.resolve();
  BOOST_TEST(resolved);
}

BOOST_AUTO_TEST_CASE(deferred_then_void_error) {
  deferred<bool> sema;
  deferred<void> v;
  deferred<int> i = v.then([sema]() mutable {
    deferred<int> result;
    sema.on([result](error_or<bool>) mutable { result.resolve(42); });
    return result;
  });

  std::string value;
  i.on([&value](const error_or<int> &s) {
    s.check_not_ok();
    value = s.error_message();
  });

  v.fail(stream_error::NotInitialized);
  BOOST_TEST(value.find("not initialized") != std::string::npos);
}
