#define BOOST_TEST_MODULE JsonToCborTest

#include <cbor.h>
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

// TODO: remove this function and #include <cbor.h> in next PRs
cbor_item_t *deserialize_cbor(const std::string &data) {
  cbor_load_result load_result{0};
  cbor_item_t *loaded_item =
      cbor_load(reinterpret_cast<cbor_data>(data.data()), data.size(), &load_result);

  BOOST_CHECK_EQUAL(load_result.error.code, CBOR_ERR_NONE);
  CHECK_NOTNULL(loaded_item);
  CHECK_EQ(1, cbor_refcount(loaded_item));
  return loaded_item;
}

}  // namespace

BOOST_AUTO_TEST_CASE(null_test) {
  nlohmann::json j = nullptr;
  cbor_item_t *c = deserialize_cbor(sv::json_to_cbor(j));
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_null(c));
}

BOOST_AUTO_TEST_CASE(false_test) {
  nlohmann::json j = false;
  cbor_item_t *c = deserialize_cbor(sv::json_to_cbor(j));
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_bool(c));
  BOOST_CHECK(!cbor_ctrl_is_bool(c));
}

BOOST_AUTO_TEST_CASE(true_test) {
  nlohmann::json j = true;
  cbor_item_t *c = deserialize_cbor(sv::json_to_cbor(j));
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_bool(c));
  BOOST_CHECK(cbor_ctrl_is_bool(c));
}

BOOST_DATA_TEST_CASE(positive_integer_test,
                     boost::unit_test::data::make(positive_int_values)
                         ^ positive_int_widths,
                     int_value, int_width) {
  nlohmann::json j = int_value;
  cbor_item_t *c = deserialize_cbor(sv::json_to_cbor(j));
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
  cbor_item_t *c = deserialize_cbor(sv::json_to_cbor(j));
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_int(c));
  BOOST_CHECK_EQUAL(int_width, cbor_int_get_width(c));
  BOOST_CHECK_EQUAL(int_value, -1 - cbor_get_int(c));
}

BOOST_AUTO_TEST_CASE(positive_float_test) {
  nlohmann::json j = 0.23;
  cbor_item_t *c = deserialize_cbor(sv::json_to_cbor(j));
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_float(c));
  BOOST_CHECK_CLOSE(0.23, cbor_float_get_float(c), 0.0000001);
}

BOOST_AUTO_TEST_CASE(negative_float_test) {
  nlohmann::json j = -0.23;
  cbor_item_t *c = deserialize_cbor(sv::json_to_cbor(j));
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_is_float(c));
  BOOST_CHECK_CLOSE(-0.23, cbor_float_get_float(c), 0.0000001);
}

BOOST_AUTO_TEST_CASE(string_test) {
  nlohmann::json j = "hello";
  cbor_item_t *c = deserialize_cbor(sv::json_to_cbor(j));
  BOOST_CHECK_EQUAL(1, cbor_refcount(c));
  BOOST_CHECK(cbor_isa_string(c));
  auto h = cbor_string_handle(c);
  std::string result{h, h + cbor_string_length(c)};
  BOOST_CHECK_EQUAL("hello", result);
}

BOOST_AUTO_TEST_CASE(array_test) {
  nlohmann::json j = {6, 7, 8, 9};
  cbor_item_t *c = deserialize_cbor(sv::json_to_cbor(j));
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

  cbor_item_t *c = deserialize_cbor(sv::json_to_cbor(j));
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