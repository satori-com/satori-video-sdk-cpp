#define BOOST_TEST_MODULE JsonToCborTest

#include <boost/test/floating_point_comparison.hpp>
#include <boost/test/included/unit_test.hpp>

#include "base64.h"
#include "cbor_json.h"

namespace sv = satori::video;

BOOST_AUTO_TEST_CASE(null_test) {
  const uint8_t data[]{
      0b11110110 /* Major type 7, value 22 = null */
  };
  const std::string expected{data, data + sizeof(data)};
  BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(nullptr));
}

BOOST_AUTO_TEST_CASE(false_test) {
  const uint8_t data[]{
      0b11110100 /* Major type 7, value 20 = false */
  };
  const std::string expected{data, data + sizeof(data)};
  BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(false));
}

BOOST_AUTO_TEST_CASE(true_test) {
  const uint8_t data[]{
      0b11110101 /* Major type 7, value 21 = true */
  };
  const std::string expected{data, data + sizeof(data)};
  BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(true));
}

BOOST_AUTO_TEST_CASE(positive_int_no_additional_data_test) {
  {
    const uint8_t data[]{
        0b00000000 /* Major type 0, value 0 */
    };
    const std::string expected{data, data + sizeof(data)};
    BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(0));
  }
  {
    const uint8_t data[]{
        0b00010111 /* Major type 0, value 23 */
    };
    const std::string expected{data, data + sizeof(data)};
    BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(23));
  }
}

BOOST_AUTO_TEST_CASE(positive_int8_test) {
  {
    const uint8_t data[]{
        0b00011000 /* Major type 0, value 24 = additional data as uint8_t */,
        24,
    };
    const std::string expected{data, data + sizeof(data)};
    BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(24));
  }
  {
    const uint8_t data[]{
        0b00011000 /* Major type 0, value 24 = additional data as uint8_t */,
        0b11111111,
    };
    const std::string expected{data, data + sizeof(data)};
    BOOST_CHECK_EQUAL(expected, sv::json_to_cbor((1LL << 8) - 1));
  }
}

BOOST_AUTO_TEST_CASE(positive_int16_test) {
  {
    const uint8_t data[]{
        0b00011001 /* Major type 0, value 25 = additional data as uint16_t */,
        0b00000001,
        0b00000000,
    };
    const std::string expected{data, data + sizeof(data)};
    BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(1LL << 8));
  }
  {
    const uint8_t data[]{
        0b00011001 /* Major type 0, value 25 = additional data as uint16_t */,
        0b11111111,
        0b11111111,
    };
    const std::string expected{data, data + sizeof(data)};
    BOOST_CHECK_EQUAL(expected, sv::json_to_cbor((1LL << 16) - 1));
  }
}

BOOST_AUTO_TEST_CASE(positive_int32_test) {
  {
    const uint8_t data[]{
        0b00011010 /* Major type 0, value 26 = additional data as uint32_t */,
        0b00000000,
        0b00000001,
        0b00000000,
        0b00000000,
    };
    const std::string expected{data, data + sizeof(data)};
    BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(1LL << 16));
  }
  {
    const uint8_t data[]{
        0b00011010 /* Major type 0, value 26 = additional data as uint32_t */,
        0b11111111,
        0b11111111,
        0b11111111,
        0b11111111,
    };
    const std::string expected{data, data + sizeof(data)};
    BOOST_CHECK_EQUAL(expected, sv::json_to_cbor((1LL << 32) - 1));
  }
}

BOOST_AUTO_TEST_CASE(positive_int64_test) {
  const uint8_t data[]{
      0b00011011 /* Major type 0, value 27 = additional data as uint64_t */,
      0b00000000,
      0b00000000,
      0b00000000,
      0b00000001,
      0b00000000,
      0b00000000,
      0b00000000,
      0b00000000,
  };
  const std::string expected{data, data + sizeof(data)};
  BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(1LL << 32));
}

BOOST_AUTO_TEST_CASE(negative_int_no_additional_data_test) {
  {
    const uint8_t data[]{
        0b00100000 /* Major type 1, value -1 */
    };
    const std::string expected{data, data + sizeof(data)};
    BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(-1));
  }
  {
    const uint8_t data[]{
        0b00110111 /* Major type 1, value -24 */
    };
    const std::string expected{data, data + sizeof(data)};
    BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(-24));
  }
}

BOOST_AUTO_TEST_CASE(negative_int8_test) {
  {
    const uint8_t data[]{
        0b00111000 /* Major type 1, value 24 = additional data as uint8_t */,
        24,
    };
    const std::string expected{data, data + sizeof(data)};
    BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(-25));
  }
  {
    const uint8_t data[]{
        0b00111000 /* Major type 1, value 24 = additional data as uint8_t */,
        0b11111111,
    };
    const std::string expected{data, data + sizeof(data)};
    BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(-(1LL << 8)));
  }
}

BOOST_AUTO_TEST_CASE(negative_int16_test) {
  {
    const uint8_t data[]{
        0b00111001 /* Major type 1, value 25 = additional data as uint16_t */,
        0b00000001,
        0b00000000,
    };
    const std::string expected{data, data + sizeof(data)};
    BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(-(1LL << 8) - 1));
  }
  {
    const uint8_t data[]{
        0b00111001 /* Major type 1, value 25 = additional data as uint16_t */,
        0b11111111,
        0b11111111,
    };
    const std::string expected{data, data + sizeof(data)};
    BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(-(1LL << 16)));
  }
}

BOOST_AUTO_TEST_CASE(negative_int32_test) {
  {
    const uint8_t data[]{
        0b00111010 /* Major type 1, value 26 = additional data as uint32_t */,
        0b00000000,
        0b00000001,
        0b00000000,
        0b00000000,
    };
    const std::string expected{data, data + sizeof(data)};
    BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(-(1LL << 16) - 1));
  }
  {
    const uint8_t data[]{
        0b00111010 /* Major type 1, value 26 = additional data as uint32_t */,
        0b11111111,
        0b11111111,
        0b11111111,
        0b11111111,
    };
    const std::string expected{data, data + sizeof(data)};
    BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(-(1LL << 32)));
  }
}

BOOST_AUTO_TEST_CASE(negative_int64_test) {
  const uint8_t data[]{
      0b00111011 /* Major type 1, value 27 = additional data as uint64_t */,
      0b00000000,
      0b00000000,
      0b00000000,
      0b00000001,
      0b00000000,
      0b00000000,
      0b00000000,
      0b00000000,
  };
  const std::string expected{data, data + sizeof(data)};
  BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(-(1LL << 32) - 1));
}

BOOST_AUTO_TEST_CASE(positive_float_test) {
  const uint8_t data[]{
      0b11111011 /* Major type 7, value 27 = double-precision float */,
      0b00111111,
      0b11110000,
      0b00000000,
      0b00000000,
      0b00000000,
      0b00000000,
      0b00000000,
      0b00000001,
  };  // 1 + 2^−52
  const std::string expected{data, data + sizeof(data)};
  BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(1 + std::pow(2, -52)));
}

BOOST_AUTO_TEST_CASE(negative_float_test) {
  const uint8_t data[]{
      0b11111011 /* Major type 7, value 27 = double-precision float */,
      0b10111111,
      0b11110000,
      0b00000000,
      0b00000000,
      0b00000000,
      0b00000000,
      0b00000000,
      0b00000001,
  };  // -(1 + 2^−52)
  const std::string expected{data, data + sizeof(data)};
  BOOST_CHECK_EQUAL(expected, sv::json_to_cbor(-1 - std::pow(2, -52)));
}

BOOST_AUTO_TEST_CASE(bytestring_test) {
  // TODO: base64 hardcoded fields "b" and "codecData"
  const uint8_t data[]{
      0b10100001 /* Major type 5, value 4 = map with 1 entry */,
      0b01100001 /* string of size 1 */,
      'b',
      0b01000011 /* Major type 2, definite-length bytestring of size 3 */,
      'a',
      'b',
      'c',
  };
  const std::string expected{data, data + sizeof(data)};
  const std::string value = sv::base64::encode("abc");
  BOOST_CHECK_EQUAL(expected, sv::json_to_cbor({{"b", value}}));
}

BOOST_AUTO_TEST_CASE(string_test) {
  const uint8_t data[]{
      0b01100100 /* Major type 3, definite-length string of size 4 */, 'a', 'b', 'c', 'd',
  };
  const std::string expected{data, data + sizeof(data)};
  BOOST_CHECK_EQUAL(expected, sv::json_to_cbor("abcd"));
}

BOOST_AUTO_TEST_CASE(array_test) {
  const uint8_t data[]{
      0b10000011 /* Major type 4, value 3 = array with 3 items */,
      0b01100001 /* string of size 1 */,
      'a',
      0b01100001 /* string of size 1 */,
      'b',
      0b01100001 /* string of size 1 */,
      'c',
  };
  const std::string expected{data, data + sizeof(data)};
  BOOST_CHECK_EQUAL(expected, sv::json_to_cbor({"a", "b", "c"}));
}

BOOST_AUTO_TEST_CASE(object_test) {
  const uint8_t data[]{
      0b10100100 /* Major type 5, value 4 = map with 4 entries */,
      0b01100001 /* string of size 1 */,
      'b',
      0b11110101 /* Major type 7, value 21 = true */,
      0b01100001 /* string of size 1 */,
      'l',
      0b10000010 /* array with 2 items */,
      0b00000000 /* 0 */,
      0b00000001 /* 1 */,
      0b01100001 /* string of size 1 */,
      'n',
      0b11110110 /* Major type 7, value 22 = null */,
      0b01100001 /* string of size 1 */,
      's',
      0b01100010 /* string of size 2 */,
      'a',
      'b',
  };
  const std::string expected{data, data + sizeof(data)};
  BOOST_CHECK_EQUAL(
      expected,
      sv::json_to_cbor({{"b", true}, {"l", {0, 1}}, {"n", nullptr}, {"s", "ab"}}));
}