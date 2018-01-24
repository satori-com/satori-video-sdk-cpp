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
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_null());
}

BOOST_AUTO_TEST_CASE(false_test) {
  const uint8_t data[]{
      0b11110100 /* Major type 7, value 20 = false */
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_boolean());
  BOOST_CHECK_EQUAL(false, j.get<bool>());
}

BOOST_AUTO_TEST_CASE(true_test) {
  const uint8_t data[]{
      0b11110101 /* Major type 7, value 21 = true */
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_boolean());
  BOOST_CHECK_EQUAL(true, j.get<bool>());
}

BOOST_AUTO_TEST_CASE(positive_int_no_additional_data_test) {
  {
    const uint8_t data[]{
        0b00000000 /* Major type 0, value 0 */
    };
    sv::streams::error_or<nlohmann::json> result =
        sv::cbor_to_json(std::string{data, data + sizeof(data)});
    BOOST_CHECK(result.ok());
    const nlohmann::json &j = result.get();
    BOOST_CHECK(j.is_number_unsigned());
    BOOST_CHECK_EQUAL(0, j.get<uint8_t>());
  }
  {
    const uint8_t data[]{
        0b00010111 /* Major type 0, value 23 */
    };
    sv::streams::error_or<nlohmann::json> result =
        sv::cbor_to_json(std::string{data, data + sizeof(data)});
    BOOST_CHECK(result.ok());
    const nlohmann::json &j = result.get();
    BOOST_CHECK(j.is_number_unsigned());
    BOOST_CHECK_EQUAL(23, j.get<uint8_t>());
  }
}

BOOST_AUTO_TEST_CASE(positive_int8_test) {
  const uint8_t data[]{
      0b00011000 /* Major type 0, value 24 = additional data as uint8_t */,
      24,
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_unsigned());
  BOOST_CHECK_EQUAL(24, j.get<uint8_t>());
}

BOOST_AUTO_TEST_CASE(positive_int16_test) {
  const uint8_t data[]{
      0b00011001 /* Major type 0, value 25 = additional data as uint16_t */,
      0b00000001,
      0b00000000,
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_unsigned());
  BOOST_CHECK_EQUAL(1LL << 8, j.get<uint16_t>());
}

BOOST_AUTO_TEST_CASE(positive_int32_test) {
  const uint8_t data[]{
      0b00011010 /* Major type 0, value 26 = additional data as uint32_t */,
      0b00000001,
      0b00000000,
      0b00000000,
      0b00000000,
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_unsigned());
  BOOST_CHECK_EQUAL(1LL << 24, j.get<uint32_t>());
}

BOOST_AUTO_TEST_CASE(positive_int64_test) {
  const uint8_t data[]{
      0b00011011 /* Major type 0, value 27 = additional data as uint64_t */,
      0b00000001,
      0b00000000,
      0b00000000,
      0b00000000,
      0b00000000,
      0b00000000,
      0b00000000,
      0b00000000,
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_unsigned());
  BOOST_CHECK_EQUAL(1LL << 56, j.get<uint64_t>());
}

BOOST_AUTO_TEST_CASE(negative_int_no_additional_data_test) {
  {
    const uint8_t data[]{
        0b00100000 /* Major type 1, value -1 */
    };
    sv::streams::error_or<nlohmann::json> result =
        sv::cbor_to_json(std::string{data, data + sizeof(data)});
    BOOST_CHECK(result.ok());
    const nlohmann::json &j = result.get();
    BOOST_CHECK(j.is_number_integer());
    BOOST_CHECK(!j.is_number_unsigned());
    BOOST_CHECK_EQUAL(-1, j.get<int8_t>());
  }
  {
    const uint8_t data[]{
        0b00110111 /* Major type 0, value -24 */
    };
    sv::streams::error_or<nlohmann::json> result =
        sv::cbor_to_json(std::string{data, data + sizeof(data)});
    BOOST_CHECK(result.ok());
    const nlohmann::json &j = result.get();
    BOOST_CHECK(j.is_number_integer());
    BOOST_CHECK(!j.is_number_unsigned());
    BOOST_CHECK_EQUAL(-24, j.get<int8_t>());
  }
}

BOOST_AUTO_TEST_CASE(negative_int8_test) {
  const uint8_t data[]{
      0b00111000 /* Major type 1, value 24 = additional data as uint8_t */,
      24,
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_integer());
  BOOST_CHECK(!j.is_number_unsigned());
  BOOST_CHECK_EQUAL(-25, j.get<int8_t>());
}

BOOST_AUTO_TEST_CASE(negative_int16_test) {
  const uint8_t data[]{
      0b00111001 /* Major type 1, value 25 = additional data as uint16_t */,
      0b00000001,
      0b00000000,
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_integer());
  BOOST_CHECK(!j.is_number_unsigned());
  BOOST_CHECK_EQUAL(-(1LL << 8) - 1, j.get<int16_t>());
}

BOOST_AUTO_TEST_CASE(negative_int32_test) {
  const uint8_t data[]{
      0b00111010 /* Major type 1, value 26 = additional data as uint32_t */,
      0b00000001,
      0b00000000,
      0b00000000,
      0b00000000,
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_integer());
  BOOST_CHECK(!j.is_number_unsigned());
  BOOST_CHECK_EQUAL(-(1LL << 24) - 1, j.get<int32_t>());
}

BOOST_AUTO_TEST_CASE(negative_int64_test) {
  const uint8_t data[]{
      0b00111011 /* Major type 1, value 27 = additional data as uint64_t */,
      0b00000001,
      0b00000000,
      0b00000000,
      0b00000000,
      0b00000000,
      0b00000000,
      0b00000000,
      0b00000000,
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_integer());
  BOOST_CHECK(!j.is_number_unsigned());
  BOOST_CHECK_EQUAL(-(1LL << 56) - 1, j.get<int64_t>());
}

BOOST_AUTO_TEST_CASE(positive_float2_test) {
  const uint8_t data[]{
      0b11111001 /* Major type 7, value 25 = half-precision float */,
      0b00111100,
      0b00000001,
  };  // 1 + 2^−10
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_float());
  BOOST_CHECK_CLOSE(1 + std::pow(2, -10), j.get<float>(), 0.000001);
}

BOOST_AUTO_TEST_CASE(positive_float4_test) {
  const uint8_t data[]{
      0b11111010 /* Major type 7, value 26 = single-precision float */,
      0b00111111,
      0b10000000,
      0b00000000,
      0b00000001,
  };  // 1 + 2^−23
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_float());
  BOOST_CHECK_CLOSE(1 + std::pow(2, -23), j.get<float>(), 0.000000001);
}

BOOST_AUTO_TEST_CASE(positive_float8_test) {
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
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_float());
  BOOST_CHECK_CLOSE(1 + std::pow(2, -52), j.get<double>(), 0.0000000000001);
}

BOOST_AUTO_TEST_CASE(negative_float2_test) {
  const uint8_t data[]{
      0b11111001 /* Major type 7, value 25 = half-precision float */,
      0b10111100,
      0b00000001,
  };  // -(1 + 2^−10)
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_float());
  BOOST_CHECK_CLOSE(-1 - std::pow(2, -10), j.get<float>(), 0.000001);
}

BOOST_AUTO_TEST_CASE(negative_float4_test) {
  const uint8_t data[]{
      0b11111010 /* Major type 7, value 26 = single-precision float */,
      0b10111111,
      0b10000000,
      0b00000000,
      0b00000001,
  };  // -(1 + 2^−23)
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_float());
  BOOST_CHECK_CLOSE(-1 - std::pow(2, -23), j.get<float>(), 0.000000001);
}

BOOST_AUTO_TEST_CASE(negative_float8_test) {
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
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_float());
  BOOST_CHECK_CLOSE(-1 - std::pow(2, -52), j.get<double>(), 0.0000000000001);
}

BOOST_AUTO_TEST_CASE(definite_length_bytestring_test) {
  const uint8_t data[]{
      0b01000100 /* Major type 2, definite-length bytestring of size 4 */,
      'a',
      'b',
      'c',
      'd',
  };
  const auto result = sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_string());
  const std::string expected = sv::base64::encode("abcd");
  BOOST_CHECK_EQUAL(expected, j.get<std::string>());
}

BOOST_AUTO_TEST_CASE(indefinite_length_bytestring_test) {
  const uint8_t data[]{
      0b01011111 /* Major type 2, value 31 = indefinite-length bytestring */,
      0b01000001 /* chunk of size 1 */,
      'a',
      0b01000010 /* chunk of size 2 */,
      'b',
      'c',
      0b11111111 /* end of bytestring */,
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_string());
  const std::string expected = sv::base64::encode("abc");
  BOOST_CHECK_EQUAL(expected, j.get<std::string>());
}

BOOST_AUTO_TEST_CASE(definite_length_string_test) {
  const uint8_t data[]{
      0b01100100 /* Major type 3, definite-length string of size 4 */, 'a', 'b', 'c', 'd',
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_string());
  BOOST_CHECK_EQUAL("abcd", j.get<std::string>());
}

BOOST_AUTO_TEST_CASE(indefinite_length_string_test) {
  const uint8_t data[]{
      0b01111111 /* Major type 3, value 31 = indefinite-length string */,
      0b01100001 /* chunk of size 1 */,
      'a',
      0b01100010 /* chunk of size 2 */,
      'b',
      'c',
      0b11111111 /* end of string */,
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_string());
  BOOST_CHECK_EQUAL("abc", j.get<std::string>());
}

BOOST_AUTO_TEST_CASE(definite_length_array_test) {
  const uint8_t data[]{
      0b10000011 /* Major type 4, value 3 = array with 3 items */,
      0b01100001 /* string of size 1 */,
      'a',
      0b01100001 /* string of size 1 */,
      'b',
      0b01100001 /* string of size 1 */,
      'c',
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_array());
  BOOST_CHECK_EQUAL(3, j.size());
  BOOST_CHECK_EQUAL("a", j[0].get<std::string>());
  BOOST_CHECK_EQUAL("b", j[1].get<std::string>());
  BOOST_CHECK_EQUAL("c", j[2].get<std::string>());
}

BOOST_AUTO_TEST_CASE(indefinite_length_array_test) {
  const uint8_t data[]{
      0b10011111 /* Major type 4, value 31 = indefinite-length array */,
      0b01100001 /* string of size 1 */,
      'a',
      0b01100001 /* string of size 1 */,
      'b',
      0b11111111 /* end of array */,
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_array());
  BOOST_CHECK_EQUAL(2, j.size());
  BOOST_CHECK_EQUAL("a", j[0].get<std::string>());
  BOOST_CHECK_EQUAL("b", j[1].get<std::string>());
}

BOOST_AUTO_TEST_CASE(definite_length_map_test) {
  const uint8_t data[]{
      0b10100100 /* Major type 5, value 4 = map with 4 entries */,
      0b01100001 /* string of size 1 */,
      's',
      0b01100010 /* string of size 2 */,
      'a',
      'b',
      0b01100001 /* string of size 1 */,
      'i',
      0b00011000 /* uint8_t is following with value 50 */,
      50,
      0b01100001 /* string of size 1 */,
      'l',
      0b10000010 /* array with 2 items */,
      0b00000000 /* 0 */,
      0b00000001 /* 1 */,
      0b01100001 /* string of size 1 */,
      'o',
      0b10100001 /* map with 1 entry */,
      0b01100001 /* string of size 1 */,
      's',
      0b01100010 /* string of size 2 */,
      'd',
      'e',
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_object());
  BOOST_CHECK_EQUAL(4, j.size());
  BOOST_CHECK_EQUAL("ab", j["s"]);
  BOOST_CHECK_EQUAL(50, j["i"]);
  BOOST_CHECK_EQUAL(0, j["l"][0]);
  BOOST_CHECK_EQUAL(1, j["l"][1]);
  BOOST_CHECK_EQUAL("de", j["o"]["s"]);
}

BOOST_AUTO_TEST_CASE(indefinite_length_map_test) {
  const uint8_t data[]{
      0b10111111 /* Major type 5, value 31 = indefinite-length map */,
      0b01100001 /* string of size 1 */,
      's',
      0b01100010 /* string of size 2 */,
      'a',
      'b',
      0b01100001 /* string of size 1 */,
      'i',
      0b00011000 /* uint8_t is following with value 50 */,
      50,
      0b11111111 /* end of map */,
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_object());
  BOOST_CHECK_EQUAL(2, j.size());
  BOOST_CHECK_EQUAL("ab", j["s"]);
  BOOST_CHECK_EQUAL(50, j["i"]);
}

BOOST_AUTO_TEST_CASE(empty_array) {
  const uint8_t data[]{
      0b10111111 /* Major type 5, value 31 = indefinite-length map */,
      0b01100001 /* string of size 1 */,
      'o',
      0b10011111 /* Major type 4, value 31 = indefinite-length array */,
      0b11111111 /* end of array */,
      0b11111111 /* end of map */,
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();

  auto &objects = j["o"];

  BOOST_CHECK(!objects.is_null());
  BOOST_CHECK(objects.is_array());
  BOOST_CHECK_EQUAL(0, objects.size());
}

BOOST_AUTO_TEST_CASE(empty_map) {
  const uint8_t data[]{
      0b10111111 /* Major type 5, value 31 = indefinite-length map */,
      0b01100001 /* string of size 1 */,
      'd',
      0b10111111 /* Major type 5, value 31 = indefinite-length map */,
      0b11111111 /* end of map */,
      0b11111111 /* end of map */,
  };
  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();

  auto &detection = j["d"];

  BOOST_CHECK(!detection.is_null());
  BOOST_CHECK(detection.is_object());
  BOOST_CHECK_EQUAL(0, detection.size());
}

BOOST_AUTO_TEST_CASE(bad_cbor) {
  const uint8_t data[]{
      0b10000001 /* array with 1 item, but no item is following */
  };

  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(!result.ok());
}
