#define BOOST_TEST_MODULE EncodingTest
#include <boost/test/included/unit_test.hpp>
#include <gsl/gsl>

#include "base64.h"

namespace sv = satori::video;

namespace {

constexpr char plain[] = "\0\0\0\x01gM\0)\x80Kp\x10\x10\x1a ADT\0\0\0\x01h<";
const std::string binary_string = std::string{plain, sizeof(plain)};

}  // namespace

BOOST_AUTO_TEST_CASE(base64_encode) {
  BOOST_CHECK_EQUAL("AAAAAWdNACmAS3AQEBogQURUAAAAAWg8AA==",
                    sv::base64::encode(binary_string));
}

BOOST_AUTO_TEST_CASE(base64_decode) {
  const auto data_or_error = sv::base64::decode("AAAAAWdNACmAS3AQEBogQURUAAAAAWg8AA==");
  BOOST_CHECK(data_or_error.ok());
  BOOST_CHECK_EQUAL(binary_string, data_or_error.get());
}

BOOST_AUTO_TEST_CASE(base64_decode_bad_value) {
  const auto data_or_error = sv::base64::decode(binary_string);
  BOOST_CHECK(!data_or_error.ok());
}

BOOST_AUTO_TEST_CASE(base64_encode_decode) {
  BOOST_CHECK_EQUAL("a", sv::base64::decode(sv::base64::encode("a")).get());
  BOOST_CHECK_EQUAL("ab", sv::base64::decode(sv::base64::encode("ab")).get());
  BOOST_CHECK_EQUAL("abc", sv::base64::decode(sv::base64::encode("abc")).get());
  BOOST_CHECK_EQUAL("abcd", sv::base64::decode(sv::base64::encode("abcd")).get());
  BOOST_CHECK_EQUAL("abcde", sv::base64::decode(sv::base64::encode("abcde")).get());
  BOOST_CHECK_EQUAL("abcdef", sv::base64::decode(sv::base64::encode("abcdef")).get());
}
