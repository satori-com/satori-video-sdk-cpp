#define BOOST_TEST_MODULE JsonToCborTest
#include <boost/test/included/unit_test.hpp>

#include "cbor_json.h"

namespace sv = satori::video;

BOOST_AUTO_TEST_CASE(null_test) {
  cbor_item_t *c = cbor_new_null();
  rapidjson::Document document;
  rapidjson::Value j = sv::cbor_to_json(c, document);
  BOOST_CHECK(j.IsNull());
}

BOOST_AUTO_TEST_CASE(false_test) {
  cbor_item_t *c = cbor_build_bool(false);
  rapidjson::Document document;
  rapidjson::Value j = sv::cbor_to_json(c, document);
  BOOST_CHECK(j.IsBool());
  BOOST_CHECK_EQUAL(false, j.GetBool());
}

BOOST_AUTO_TEST_CASE(true_test) {
  cbor_item_t *c = cbor_build_bool(true);
  rapidjson::Document document;
  rapidjson::Value j = sv::cbor_to_json(c, document);
  BOOST_CHECK(j.IsBool());
  BOOST_CHECK_EQUAL(true, j.GetBool());
}
