#define BOOST_TEST_MODULE JsonToCborTest

#include <list>

#include <boost/test/data/test_case.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <boost/test/included/unit_test.hpp>

#include "cbor_json.h"

namespace sv = satori::video;

namespace {

int64_t positive_int_values[] = {
    0,           (1LL << 8) - 1,  (1LL << 8), (1LL << 16) - 1,
    (1LL << 16), (1LL << 32) - 1, (1LL << 32)};
cbor_int_width positive_int_widths[] = {CBOR_INT_8,  CBOR_INT_8,  CBOR_INT_16,
                                        CBOR_INT_16, CBOR_INT_32, CBOR_INT_32,
                                        CBOR_INT_64};

int64_t negative_int_values[] = {
    -1,           -(1LL << 8),     -(1LL << 8) - 1, -(1LL << 16), -(1LL << 16) - 1,
    -(1LL << 32), -(1LL << 32) - 1};
cbor_int_width negative_int_widths[] = {CBOR_INT_8,  CBOR_INT_8,  CBOR_INT_16,
                                        CBOR_INT_16, CBOR_INT_32, CBOR_INT_32,
                                        CBOR_INT_64};

}  // namespace

BOOST_AUTO_TEST_CASE(rapidjson_null_test) {
  rapidjson::Value j;
  cbor_item_t *c = sv::rapidjson_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_null(c));
}

BOOST_AUTO_TEST_CASE(rapidjson_false_test) {
  rapidjson::Value j(false);
  cbor_item_t *c = sv::rapidjson_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_bool(c));
  BOOST_CHECK(!cbor_ctrl_is_bool(c));
}

BOOST_AUTO_TEST_CASE(rapidjson_true_test) {
  rapidjson::Value j(true);
  cbor_item_t *c = sv::rapidjson_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_bool(c));
  BOOST_CHECK(cbor_ctrl_is_bool(c));
}

BOOST_AUTO_TEST_CASE(rapidjson_pos_int_test) {
  rapidjson::Value j;
  j.SetInt(0);
  cbor_item_t *c = sv::rapidjson_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_int(c));
  BOOST_CHECK(!cbor_isa_negint(c));
  BOOST_CHECK_EQUAL(CBOR_INT_32, cbor_int_get_width(c));
  BOOST_CHECK_EQUAL(0, cbor_get_int(c));
}

BOOST_AUTO_TEST_CASE(rapidjson_pos_int64_test) {
  rapidjson::Value j;
  j.SetInt64(1LL << 32);
  cbor_item_t *c = sv::rapidjson_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_int(c));
  BOOST_CHECK(!cbor_isa_negint(c));
  BOOST_CHECK_EQUAL(CBOR_INT_64, cbor_int_get_width(c));
  BOOST_CHECK_EQUAL(1LL << 32, cbor_get_int(c));
}

BOOST_AUTO_TEST_CASE(rapidjson_neg_int_test) {
  rapidjson::Value j;
  j.SetInt(-1);
  cbor_item_t *c = sv::rapidjson_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_int(c));
  BOOST_CHECK(cbor_isa_negint(c));
  BOOST_CHECK_EQUAL(CBOR_INT_32, cbor_int_get_width(c));
  BOOST_CHECK_EQUAL(0, cbor_get_int(c));
}

BOOST_AUTO_TEST_CASE(rapidjson_neg_int64_test) {
  rapidjson::Value j;
  j.SetInt64(-(1LL << 32) - 1LL);
  cbor_item_t *c = sv::rapidjson_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_int(c));
  BOOST_CHECK(cbor_isa_negint(c));
  BOOST_CHECK_EQUAL(CBOR_INT_64, cbor_int_get_width(c));
  BOOST_CHECK_EQUAL(1LL << 32, cbor_get_int(c));
}

BOOST_AUTO_TEST_CASE(rapidjson_string_test) {
  rapidjson::Value j("hello");
  cbor_item_t *c = sv::rapidjson_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_isa_string(c));
  auto h = cbor_string_handle(c);
  std::string result{h, h + cbor_string_length(c)};
  BOOST_CHECK_EQUAL("hello", result);
}

BOOST_AUTO_TEST_CASE(rapidjson_array_test) {
  rapidjson::Document d;
  rapidjson::Value j(rapidjson::kArrayType);
  j.PushBack(rapidjson::Value(6), d.GetAllocator());
  j.PushBack(rapidjson::Value(7), d.GetAllocator());
  j.PushBack(rapidjson::Value(8), d.GetAllocator());
  j.PushBack(rapidjson::Value(9), d.GetAllocator());

  cbor_item_t *c = sv::rapidjson_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_isa_array(c));
  auto h = cbor_array_handle(c);

  BOOST_CHECK_EQUAL(4, cbor_array_size(c));
  BOOST_CHECK_EQUAL(6, cbor_get_int(h[0]));
  BOOST_CHECK_EQUAL(7, cbor_get_int(h[1]));
  BOOST_CHECK_EQUAL(8, cbor_get_int(h[2]));
  BOOST_CHECK_EQUAL(9, cbor_get_int(h[3]));
}

BOOST_AUTO_TEST_CASE(rapidjson_map_test) {
  rapidjson::Document d;
  rapidjson::Value j(rapidjson::kObjectType);
  j.AddMember(rapidjson::Value("key1"), rapidjson::Value(6), d.GetAllocator());
  j.AddMember(rapidjson::Value("key2"), rapidjson::Value("dummy"), d.GetAllocator());

  cbor_item_t *c = sv::rapidjson_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_isa_map(c));
  auto h = cbor_map_handle(c);

  BOOST_CHECK_EQUAL(2, cbor_map_size(c));
  for (size_t i = 0; i < cbor_map_size(c); i++) {
    const std::string key{cbor_string_handle(h[i].key),
                          cbor_string_handle(h[i].key) + cbor_string_length(h[i].key)};

    if (key == "key1") {
      BOOST_CHECK(cbor_is_int(h[i].value));
    } else if (key == "key2") {
      BOOST_CHECK(cbor_isa_string(h[i].value));
    } else {
      BOOST_CHECK(false);
    }
  }
}

BOOST_AUTO_TEST_CASE(null_test) {
  nlohmann::json j = nullptr;
  cbor_item_t *c = sv::json_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_null(c));
}

BOOST_AUTO_TEST_CASE(false_test) {
  nlohmann::json j = false;
  cbor_item_t *c = sv::json_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_bool(c));
  BOOST_CHECK(!cbor_ctrl_is_bool(c));
}

BOOST_AUTO_TEST_CASE(true_test) {
  nlohmann::json j = true;
  cbor_item_t *c = sv::json_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_bool(c));
  BOOST_CHECK(cbor_ctrl_is_bool(c));
}

BOOST_DATA_TEST_CASE(positive_integer_test,
                     boost::unit_test::data::make(positive_int_values)
                         ^ positive_int_widths,
                     int_value, int_width) {
  nlohmann::json j = int_value;
  cbor_item_t *c = sv::json_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_int(c));
  BOOST_CHECK_EQUAL(int_width, cbor_int_get_width(c));
  BOOST_CHECK_EQUAL(int_value, cbor_get_int(c));
}

BOOST_DATA_TEST_CASE(negative_integer_test,
                     boost::unit_test::data::make(negative_int_values)
                         ^ negative_int_widths,
                     int_value, int_width) {
  nlohmann::json j = int_value;
  cbor_item_t *c = sv::json_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_int(c));
  BOOST_CHECK_EQUAL(int_width, cbor_int_get_width(c));
  BOOST_CHECK_EQUAL(int_value, -1 - cbor_get_int(c));
}

BOOST_AUTO_TEST_CASE(positive_float_test) {
  nlohmann::json j = 0.23;
  cbor_item_t *c = sv::json_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_float(c));
  BOOST_CHECK_CLOSE(0.23, cbor_float_get_float(c), 0.0000001);
}

BOOST_AUTO_TEST_CASE(negative_float_test) {
  nlohmann::json j = -0.23;
  cbor_item_t *c = sv::json_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_float(c));
  BOOST_CHECK_CLOSE(-0.23, cbor_float_get_float(c), 0.0000001);
}

BOOST_AUTO_TEST_CASE(string_test) {
  nlohmann::json j = "hello";
  cbor_item_t *c = sv::json_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_isa_string(c));
  auto h = cbor_string_handle(c);
  std::string result{h, h + cbor_string_length(c)};
  BOOST_CHECK_EQUAL("hello", result);
}

BOOST_AUTO_TEST_CASE(array_test) {
  nlohmann::json j = {6, 7, 8, 9};
  cbor_item_t *c = sv::json_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_isa_array(c));
  auto h = cbor_array_handle(c);

  BOOST_CHECK_EQUAL(4, cbor_array_size(c));
  BOOST_CHECK_EQUAL(6, cbor_get_int(h[0]));
  BOOST_CHECK_EQUAL(7, cbor_get_int(h[1]));
  BOOST_CHECK_EQUAL(8, cbor_get_int(h[2]));
  BOOST_CHECK_EQUAL(9, cbor_get_int(h[3]));
}

BOOST_AUTO_TEST_CASE(object_test) {
  nlohmann::json j = {
      {"pi", 3.141},       {"happy", true},
      {"name", "Niels"},   {"nothing", nullptr},
      {"list", {1, 0, 2}}, {"object", {{"currency", "USD"}, {"value", 42.99}}}};

  cbor_item_t *c = sv::json_to_cbor(j);
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_isa_map(c));
  auto h = cbor_map_handle(c);
  BOOST_CHECK_EQUAL(6, cbor_map_size(c));
  for (size_t i = 0; i < cbor_map_size(c); i++) {
    const std::string key{cbor_string_handle(h[i].key),
                          cbor_string_handle(h[i].key) + cbor_string_length(h[i].key)};

    if (key == "pi") {
      BOOST_CHECK(cbor_is_float(h[i].value));
    } else if (key == "happy") {
      BOOST_CHECK(cbor_is_bool(h[i].value));
    } else if (key == "name") {
      BOOST_CHECK(cbor_isa_string(h[i].value));
    } else if (key == "nothing") {
      BOOST_CHECK(cbor_is_null(h[i].value));
    } else if (key == "list") {
      BOOST_CHECK(cbor_isa_array(h[i].value));
    } else if (key == "object") {
      BOOST_CHECK(cbor_isa_map(h[i].value));
    } else {
      BOOST_CHECK(false);
    }
  }
}