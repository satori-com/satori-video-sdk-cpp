#define BOOST_TEST_MODULE ThreadUtilsTest
#include <boost/test/included/unit_test.hpp>

#include "threadutils.h"

namespace sv = satori::video;

BOOST_AUTO_TEST_CASE(basic) {
  sv::threadutils::set_current_thread_name("test1");
  BOOST_CHECK_EQUAL("test1", sv::threadutils::get_current_thread_name());

  sv::threadutils::set_current_thread_name("test2");
  BOOST_CHECK_EQUAL("test2", sv::threadutils::get_current_thread_name());
}

BOOST_AUTO_TEST_CASE(long_name) {
  sv::threadutils::set_current_thread_name("asdfasdfasdfasdf");
  BOOST_CHECK_EQUAL("asdfasdfasdfasd", sv::threadutils::get_current_thread_name());
}
