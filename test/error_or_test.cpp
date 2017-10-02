#define BOOST_TEST_MODULE ErrorOrTest
#include <boost/test/included/unit_test.hpp>

#include "error_or.h"

using namespace rtm::video;

BOOST_AUTO_TEST_CASE(error_or_conversions) {
  error_or<int> i1(10);
  i1.check_ok();

  error_or<int> i2 = std::error_condition(video_error::EndOfStreamError);
  i2.check_not_ok();

  const int i = -100;
  error_or<int> i3 = i;
  i3.check_ok();
}

BOOST_AUTO_TEST_CASE(error_or_move_test) {
  std::unique_ptr<int> ptr(new int);
  CHECK_NOTNULL(ptr);

  error_or<std::unique_ptr<int>> status(std::move(ptr));
  status.check_ok();
  CHECK(ptr == nullptr);

  ptr = status.move();
  CHECK_NOTNULL(ptr);
  status.check_not_ok();
}

BOOST_AUTO_TEST_CASE(error_or_constructor_destructor_test) {
  static int constructor = 0;
  static int destructor = 0;

  struct foo {
    bool active{true};

    foo() { constructor++; }
    foo(const foo &) = delete;
    foo(foo &&other) { other.active = false; }

    ~foo() {
      if (active) destructor++;
    }
  };

  CHECK_EQ(constructor, 0);
  CHECK_EQ(destructor, 0);

  std::unique_ptr<error_or<foo>> ptr;
  CHECK_EQ(constructor, 0);
  CHECK_EQ(destructor, 0);

  foo f;
  CHECK_EQ(constructor, 1);
  CHECK_EQ(destructor, 0);

  ptr.reset(new error_or<foo>(std::move(f)));
  CHECK_EQ(constructor, 1);
  CHECK_EQ(destructor, 0);

  ptr.reset();
  CHECK_EQ(constructor, 1);
  CHECK_EQ(destructor, 1);

  ptr.reset(new error_or<foo>(std::error_condition(video_error::FrameNotReadyError)));
  CHECK_EQ(constructor, 1);
  CHECK_EQ(destructor, 1);

  ptr.reset();
  CHECK_EQ(constructor, 1);
  CHECK_EQ(destructor, 1);
};
