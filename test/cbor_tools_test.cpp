#define BOOST_TEST_MODULE CborToolsTest
#include <boost/test/included/unit_test.hpp>

#include "cbor_tools.h"

BOOST_AUTO_TEST_CASE(to_output_stream) {
  cbor_item_t* map = cbor_new_indefinite_map();
  cbor_map_add(map, {cbor_move(cbor_build_string("action")),
                     cbor_move(cbor_build_string("configure"))});

  std::stringstream ss;

  ss << map;

  std::string result;

  ss >> result;

  BOOST_CHECK_EQUAL("{\"action\":\"configure\"}", result);
}
