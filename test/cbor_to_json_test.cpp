#define BOOST_TEST_MODULE JsonToCborTest
#include <boost/test/floating_point_comparison.hpp>
#include <boost/test/included/unit_test.hpp>

#include <cbor.h>
#include <gsl/gsl>

#include "cbor_json.h"

namespace sv = satori::video;

namespace {

// TODO: remove this function and #include <cbor.h> in next PRs
std::string serialize_cbor(const cbor_item_t *c) {
  BOOST_CHECK(c);
  uint8_t *buffer{nullptr};
  size_t buffer_size_ignore{0};
  const size_t buffer_length = cbor_serialize_alloc(c, &buffer, &buffer_size_ignore);

  CHECK_NOTNULL(buffer) << "failed to allocate cbor buffer: null";
  CHECK_GT(buffer_length, 0) << "failed to allocate cbor buffer: " << buffer_length;

  auto free_buffer = gsl::finally([buffer]() { free(buffer); });

  return std::string{buffer, buffer + buffer_length};
}

}  // namespace

BOOST_AUTO_TEST_CASE(null_test) {
  cbor_item_t *c = cbor_new_null();
  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_null());
}

BOOST_AUTO_TEST_CASE(false_test) {
  cbor_item_t *c = cbor_build_bool(false);
  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_boolean());
  BOOST_CHECK_EQUAL(false, j.get<bool>());
}

BOOST_AUTO_TEST_CASE(true_test) {
  cbor_item_t *c = cbor_build_bool(true);
  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_boolean());
  BOOST_CHECK_EQUAL(true, j.get<bool>());
}

BOOST_AUTO_TEST_CASE(positive_int8_test) {
  cbor_item_t *c = cbor_build_uint8(0);
  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_unsigned());
  BOOST_CHECK_EQUAL(0, j.get<uint8_t>());
}

BOOST_AUTO_TEST_CASE(positive_int16_test) {
  cbor_item_t *c = cbor_build_uint16((1LL << 8) + 1);
  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_unsigned());
  BOOST_CHECK_EQUAL((1LL << 8) + 1, j.get<uint16_t>());
}

BOOST_AUTO_TEST_CASE(positive_int32_test) {
  cbor_item_t *c = cbor_build_uint32((1LL << 16) + 1);
  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_unsigned());
  BOOST_CHECK_EQUAL((1LL << 16) + 1, j.get<uint32_t>());
}

BOOST_AUTO_TEST_CASE(positive_int64_test) {
  cbor_item_t *c = cbor_build_uint64((1LL << 32) + 1);
  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_unsigned());
  BOOST_CHECK_EQUAL((1LL << 32) + 1, j.get<uint64_t>());
}

BOOST_AUTO_TEST_CASE(negative_int8_test) {
  cbor_item_t *c = cbor_build_negint8(0);
  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_integer());
  BOOST_CHECK(!j.is_number_unsigned());
  BOOST_CHECK_EQUAL(-1, j.get<int8_t>());
}

BOOST_AUTO_TEST_CASE(negative_int16_test) {
  cbor_item_t *c = cbor_build_negint16((1LL << 8) + 1);
  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_integer());
  BOOST_CHECK(!j.is_number_unsigned());
  BOOST_CHECK_EQUAL(-(1LL << 8) - 2, j.get<int16_t>());
}

BOOST_AUTO_TEST_CASE(negative_int32_test) {
  cbor_item_t *c = cbor_build_negint32((1LL << 16) + 1);
  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_integer());
  BOOST_CHECK(!j.is_number_unsigned());
  BOOST_CHECK_EQUAL(-(1LL << 16) - 2, j.get<int32_t>());
}

BOOST_AUTO_TEST_CASE(negative_int64_test) {
  cbor_item_t *c = cbor_build_negint64((1LL << 32) + 1);
  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_integer());
  BOOST_CHECK(!j.is_number_unsigned());
  BOOST_CHECK_EQUAL(-(1LL << 32) - 2, j.get<int64_t>());
}

// TODO: enable this test when <cbor.h> is removed from this file
// BOOST_AUTO_TEST_CASE(positive_float2_test) {
//  cbor_item_t *c = cbor_build_float2(0.23f);
//  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
//  BOOST_CHECK(result.ok());
//  const nlohmann::json &j = result.get();
//  BOOST_CHECK(j.is_number_float());
//  BOOST_CHECK_CLOSE(0.23, j.get<float>(), 0.00001);
//}

BOOST_AUTO_TEST_CASE(positive_float4_test) {
  cbor_item_t *c = cbor_build_float4(0.23f);
  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_float());
  BOOST_CHECK_CLOSE(0.23, j.get<float>(), 0.00001);
}

BOOST_AUTO_TEST_CASE(positive_float8_test) {
  cbor_item_t *c = cbor_build_float8(0.23);
  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_float());
  BOOST_CHECK_CLOSE(0.23, j.get<double>(), 0.0000000000001);
}

// TODO: enable this test when <cbor.h> is removed from this file
// BOOST_AUTO_TEST_CASE(negative_float2_test) {
//  cbor_item_t *c = cbor_build_float2(-0.23f);
//  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
//  BOOST_CHECK(result.ok());
//  const nlohmann::json &j = result.get();
//  BOOST_CHECK(j.is_number_float());
//  BOOST_CHECK_CLOSE(-0.23, j.get<float>(), 0.00001);
//}

BOOST_AUTO_TEST_CASE(negative_float4_test) {
  cbor_item_t *c = cbor_build_float4(-0.23f);
  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_float());
  BOOST_CHECK_CLOSE(-0.23, j.get<float>(), 0.00001);
}

BOOST_AUTO_TEST_CASE(negative_float8_test) {
  cbor_item_t *c = cbor_build_float8(-0.23);
  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_number_float());
  BOOST_CHECK_CLOSE(-0.23, j.get<double>(), 0.0000000000001);
}

BOOST_AUTO_TEST_CASE(definite_string_test) {
  cbor_item_t *c = cbor_build_string("hello");
  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_string());
  BOOST_CHECK_EQUAL("hello", j.get<std::string>());
}

BOOST_AUTO_TEST_CASE(indefinite_string_test) {
  const uint8_t data[] = {0b01111111 /* indefinite-length string */,
                          0b01100100 /* chunk of size 4 */,
                          'a',
                          'b',
                          'c',
                          'd',
                          0b01100101 /* chunk of size 5 */,
                          'e',
                          'f',
                          'g',
                          'h',
                          'i',
                          0b11111111 /* end of string */};

  cbor_load_result load_result;
  cbor_item_t *c = cbor_load(data, sizeof(data), &load_result);
  BOOST_CHECK_EQUAL(CBOR_ERR_NONE, load_result.error.code);
  BOOST_CHECK_EQUAL(sizeof(data), load_result.read);
  BOOST_CHECK(cbor_string_is_indefinite(c));
  BOOST_CHECK_EQUAL(0, cbor_string_length(c));

  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_string());
  BOOST_CHECK_EQUAL("abcdefghi", j.get<std::string>());
}

BOOST_AUTO_TEST_CASE(array_test) {
  cbor_item_t *c = cbor_new_definite_array(3);
  cbor_array_set(c, 0, cbor_build_string("hi"));
  cbor_array_set(c, 1, cbor_build_string("there"));
  cbor_array_set(c, 2, cbor_build_string("bye"));
  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(c));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_array());
  BOOST_CHECK_EQUAL(3, j.size());
  BOOST_CHECK_EQUAL("hi", j[0].get<std::string>());
  BOOST_CHECK_EQUAL("there", j[1].get<std::string>());
  BOOST_CHECK_EQUAL("bye", j[2].get<std::string>());
}

BOOST_AUTO_TEST_CASE(map_test) {
  cbor_item_t *children = cbor_new_indefinite_array();
  cbor_array_set(children, 0, cbor_build_string("Bill"));
  cbor_array_set(children, 1, cbor_build_string("Mark"));

  cbor_item_t *car = cbor_new_indefinite_map();
  cbor_map_add(car, {cbor_build_string("make"), cbor_build_string("Honda")});
  cbor_map_add(car, {cbor_build_string("model"), cbor_build_string("Accord")});
  cbor_map_add(car, {cbor_build_string("year"), cbor_build_uint16(2000)});

  cbor_item_t *person = cbor_new_indefinite_map();
  cbor_map_add(person, {cbor_build_string("name"), cbor_build_string("Niels")});
  cbor_map_add(person, {cbor_build_string("age"), cbor_build_uint8(50)});
  cbor_map_add(person, {cbor_build_string("children"), cbor_move(children)});
  cbor_map_add(person, {cbor_build_string("car"), cbor_move(car)});

  sv::streams::error_or<nlohmann::json> result = sv::cbor_to_json(serialize_cbor(person));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();
  BOOST_CHECK(j.is_object());
  BOOST_CHECK_EQUAL(4, j.size());
  BOOST_CHECK_EQUAL("Niels", j["name"]);
  BOOST_CHECK_EQUAL(50, j["age"]);
  BOOST_CHECK_EQUAL("Bill", j["children"][0]);
  BOOST_CHECK_EQUAL("Mark", j["children"][1]);
  BOOST_CHECK_EQUAL("Honda", j["car"]["make"]);
  BOOST_CHECK_EQUAL("Accord", j["car"]["model"]);
  BOOST_CHECK_EQUAL(2000, j["car"]["year"]);
}

BOOST_AUTO_TEST_CASE(empty_array) {
  cbor_item_t *detections = cbor_new_indefinite_array();
  cbor_item_t *analysis_message = cbor_new_indefinite_map();
  cbor_map_add(analysis_message,
               {cbor_move(cbor_build_string("detected_objects")), cbor_move(detections)});

  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(serialize_cbor(analysis_message));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();

  auto &detected_objects = j["detected_objects"];

  BOOST_CHECK(!detected_objects.is_null());
  BOOST_CHECK(detected_objects.is_array());
  BOOST_CHECK_EQUAL(0, detected_objects.size());
}

BOOST_AUTO_TEST_CASE(empty_map) {
  cbor_item_t *detection = cbor_new_indefinite_map();
  cbor_item_t *analysis_message = cbor_new_indefinite_map();
  cbor_map_add(analysis_message,
               {cbor_move(cbor_build_string("detection")), cbor_move(detection)});

  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(serialize_cbor(analysis_message));
  BOOST_CHECK(result.ok());
  const nlohmann::json &j = result.get();

  auto &detected_objects = j["detection"];

  BOOST_CHECK(!detected_objects.is_null());
  BOOST_CHECK(detected_objects.is_object());
  BOOST_CHECK_EQUAL(0, detected_objects.size());
}

BOOST_AUTO_TEST_CASE(bad_cbor) {
  const uint8_t data[]{
      0b10000001 /* array with 1 item, but no item is following */
  };

  sv::streams::error_or<nlohmann::json> result =
      sv::cbor_to_json(std::string{data, data + sizeof(data)});
  BOOST_CHECK(!result.ok());
}
