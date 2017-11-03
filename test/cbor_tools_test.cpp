#define BOOST_TEST_MODULE CborToolsTest
#include <boost/test/included/unit_test.hpp>
#include <gsl/gsl>

#include "cbor_tools.h"

BOOST_AUTO_TEST_CASE(get_string) {
  cbor_item_t* item = cbor_build_string("hello");
  auto decref = gsl::finally([&item]() { cbor_decref(&item); });
  BOOST_CHECK_EQUAL("hello", cbor::get_string(item));
}

BOOST_AUTO_TEST_CASE(to_output_stream) {
  cbor_item_t* map = cbor_new_indefinite_map();
  auto decref = gsl::finally([&map]() { cbor_decref(&map); });
  cbor_map_add(map, {cbor_move(cbor_build_string("action")),
                     cbor_move(cbor_build_string("configure"))});

  std::stringstream ss;

  ss << map;

  std::string result;

  ss >> result;

  BOOST_CHECK_EQUAL("{\"action\":\"configure\"}", result);
}
