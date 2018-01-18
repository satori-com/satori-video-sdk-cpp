#define BOOST_TEST_MODULE EncodingTest
#include <boost/test/included/unit_test.hpp>
#include <gsl/gsl>

#include "base64.h"

namespace sv = satori::video;

BOOST_AUTO_TEST_CASE(base64_encode) {
  constexpr char plain[] = "\0\0\0\x01gM\0)\x80Kp\x10\x10\x1a ADT\0\0\0\x01h<";
  std::string str = std::string(plain, sizeof(plain));
  std::string result = sv::base64::encode(str);
  BOOST_CHECK_EQUAL("AAAAAWdNACmAS3AQEBogQURUAAAAAWg8AA==", result);
}
