#define BOOST_TEST_MODULE JsonToCborTest
#include <boost/test/included/unit_test.hpp>

#include "cbor_json.h"

namespace sv = satori::video;

BOOST_AUTO_TEST_CASE(rapidjson_null_test) {
  cbor_item_t *c = cbor_new_null();
  rapidjson::Document document;
  rapidjson::Value j = sv::cbor_to_rapidjson(c, document);
  BOOST_CHECK(j.IsNull());
}

BOOST_AUTO_TEST_CASE(rapidjson_false_test) {
  cbor_item_t *c = cbor_build_bool(false);
  rapidjson::Document document;
  rapidjson::Value j = sv::cbor_to_rapidjson(c, document);
  BOOST_CHECK(j.IsBool());
  BOOST_CHECK_EQUAL(false, j.GetBool());
}

BOOST_AUTO_TEST_CASE(rapidjson_true_test) {
  cbor_item_t *c = cbor_build_bool(true);
  rapidjson::Document document;
  rapidjson::Value j = sv::cbor_to_rapidjson(c, document);
  BOOST_CHECK(j.IsBool());
  BOOST_CHECK_EQUAL(true, j.GetBool());
}

BOOST_AUTO_TEST_CASE(null_test) {
  cbor_item_t *c = cbor_new_null();
  nlohmann::json j = sv::cbor_to_json(c);
  BOOST_CHECK(j.is_null());
}

BOOST_AUTO_TEST_CASE(false_test) {
  cbor_item_t *c = cbor_build_bool(false);
  nlohmann::json j = sv::cbor_to_json(c);
  BOOST_CHECK(j.is_boolean());
  BOOST_CHECK_EQUAL(false, j.get<bool>());
}

BOOST_AUTO_TEST_CASE(true_test) {
  cbor_item_t *c = cbor_build_bool(true);
  nlohmann::json j = sv::cbor_to_json(c);
  BOOST_CHECK(j.is_boolean());
  BOOST_CHECK_EQUAL(true, j.get<bool>());
}

BOOST_AUTO_TEST_CASE(positive_int8_test) {
  cbor_item_t *c = cbor_build_uint8(0);
  nlohmann::json j = sv::cbor_to_json(c);
  BOOST_CHECK(j.is_number_unsigned());
  BOOST_CHECK_EQUAL(0, j.get<uint8_t>());
}

BOOST_AUTO_TEST_CASE(positive_int16_test) {
  cbor_item_t *c = cbor_build_uint16((1LL << 8) + 1);
  nlohmann::json j = sv::cbor_to_json(c);
  BOOST_CHECK(j.is_number_unsigned());
  BOOST_CHECK_EQUAL((1LL << 8) + 1, j.get<uint16_t>());
}

BOOST_AUTO_TEST_CASE(positive_int32_test) {
  cbor_item_t *c = cbor_build_uint32((1LL << 16) + 1);
  nlohmann::json j = sv::cbor_to_json(c);
  BOOST_CHECK(j.is_number_unsigned());
  BOOST_CHECK_EQUAL((1LL << 16) + 1, j.get<uint32_t>());
}

BOOST_AUTO_TEST_CASE(positive_int64_test) {
  cbor_item_t *c = cbor_build_uint64((1LL << 32) + 1);
  nlohmann::json j = sv::cbor_to_json(c);
  BOOST_CHECK(j.is_number_unsigned());
  BOOST_CHECK_EQUAL((1LL << 32) + 1, j.get<uint64_t>());
}

BOOST_AUTO_TEST_CASE(negative_int8_test) {
  cbor_item_t *c = cbor_build_negint8(0);
  nlohmann::json j = sv::cbor_to_json(c);
  BOOST_CHECK(j.is_number_integer());
  BOOST_CHECK(!j.is_number_unsigned());
  BOOST_CHECK_EQUAL(-1, j.get<int8_t>());
}

BOOST_AUTO_TEST_CASE(negative_int16_test) {
  cbor_item_t *c = cbor_build_negint16((1LL << 8) + 1);
  nlohmann::json j = sv::cbor_to_json(c);
  BOOST_CHECK(j.is_number_integer());
  BOOST_CHECK(!j.is_number_unsigned());
  BOOST_CHECK_EQUAL(-(1LL << 8) - 2, j.get<int16_t>());
}

BOOST_AUTO_TEST_CASE(negative_int32_test) {
  cbor_item_t *c = cbor_build_negint32((1LL << 16) + 1);
  nlohmann::json j = sv::cbor_to_json(c);
  BOOST_CHECK(j.is_number_integer());
  BOOST_CHECK(!j.is_number_unsigned());
  BOOST_CHECK_EQUAL(-(1LL << 16) - 2, j.get<int32_t>());
}

BOOST_AUTO_TEST_CASE(negative_int64_test) {
  cbor_item_t *c = cbor_build_negint64((1LL << 32) + 1);
  nlohmann::json j = sv::cbor_to_json(c);
  BOOST_CHECK(j.is_number_integer());
  BOOST_CHECK(!j.is_number_unsigned());
  BOOST_CHECK_EQUAL(-(1LL << 32) - 2, j.get<int64_t>());
}

BOOST_AUTO_TEST_CASE(positive_float2_test) {
  cbor_item_t *c = cbor_build_float2(0.23f);
  nlohmann::json j = sv::cbor_to_json(c);
  BOOST_CHECK(j.is_number_float());
  BOOST_CHECK_CLOSE(0.23, j.get<float>(), 0.00001);
}

BOOST_AUTO_TEST_CASE(positive_float4_test) {
  cbor_item_t *c = cbor_build_float4(0.23f);
  nlohmann::json j = sv::cbor_to_json(c);
  BOOST_CHECK(j.is_number_float());
  BOOST_CHECK_CLOSE(0.23, j.get<float>(), 0.00001);
}

BOOST_AUTO_TEST_CASE(positive_float8_test) {
  cbor_item_t *c = cbor_build_float8(0.23);
  nlohmann::json j = sv::cbor_to_json(c);
  BOOST_CHECK(j.is_number_float());
  BOOST_CHECK_CLOSE(0.23, j.get<double>(), 0.0000000000001);
}

BOOST_AUTO_TEST_CASE(negative_float2_test) {
  cbor_item_t *c = cbor_build_float2(-0.23f);
  nlohmann::json j = sv::cbor_to_json(c);
  BOOST_CHECK(j.is_number_float());
  BOOST_CHECK_CLOSE(-0.23, j.get<float>(), 0.00001);
}

BOOST_AUTO_TEST_CASE(negative_float4_test) {
  cbor_item_t *c = cbor_build_float4(-0.23f);
  nlohmann::json j = sv::cbor_to_json(c);
  BOOST_CHECK(j.is_number_float());
  BOOST_CHECK_CLOSE(-0.23, j.get<float>(), 0.00001);
}

BOOST_AUTO_TEST_CASE(negative_float8_test) {
  cbor_item_t *c = cbor_build_float8(-0.23);
  nlohmann::json j = sv::cbor_to_json(c);
  BOOST_CHECK(j.is_number_float());
  BOOST_CHECK_CLOSE(-0.23, j.get<double>(), 0.0000000000001);
}

BOOST_AUTO_TEST_CASE(definite_string_test) {
  cbor_item_t *c = cbor_build_string("hello");
  nlohmann::json j = sv::cbor_to_json(c);
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

  nlohmann::json j = sv::cbor_to_json(c);
  BOOST_CHECK(j.is_string());
  BOOST_CHECK_EQUAL("abcdefghi", j.get<std::string>());
}

BOOST_AUTO_TEST_CASE(array_test) {
  cbor_item_t *c = cbor_new_definite_array(3);
  cbor_array_set(c, 0, cbor_build_string("hi"));
  cbor_array_set(c, 1, cbor_build_string("there"));
  cbor_array_set(c, 2, cbor_build_string("bye"));
  nlohmann::json j = sv::cbor_to_json(c);
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

  nlohmann::json j = sv::cbor_to_json(person);
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