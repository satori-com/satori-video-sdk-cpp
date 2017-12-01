#define BOOST_TEST_MODULE DataTest
#include <boost/test/included/unit_test.hpp>

#include "cbor_tools.h"
#include "data.h"
#include "logging.h"

namespace sv = satori::video;

BOOST_AUTO_TEST_CASE(encoded_metadata_to_network) {
  sv::encoded_metadata em;
  em.codec_name = "dummy-codec";
  em.codec_data = "dummy-codec-data";

  const sv::network_metadata& nm = em.to_network();

  BOOST_CHECK_EQUAL("dummy-codec", nm.codec_name);
  BOOST_CHECK_EQUAL("ZHVtbXktY29kZWMtZGF0YQ==", nm.base64_data);
}

BOOST_AUTO_TEST_CASE(encoded_metadata_to_string) {
  sv::encoded_metadata em;
  em.codec_name = "dummy-codec";
  em.codec_data = "dummy-codec-data";

  std::stringstream ss;

  ss << em;

  std::string result;

  ss >> result;

  BOOST_CHECK_EQUAL("(codec_name=dummy-codec,base64_data=ZHVtbXktY29kZWMtZGF0YQ==)",
                    result);
}

BOOST_AUTO_TEST_CASE(additional_data_test) {
  sv::encoded_metadata em;
  em.codec_name = "dummy-codec";
  em.codec_data = "dummy-codec-data";
  em.additional_data = cbor_new_indefinite_map();
  cbor_map_add(em.additional_data,
               {cbor_move(cbor_build_string("fps")), cbor_move(cbor_build_uint64(25))});

  const sv::network_metadata& nm = em.to_network();
  cbor_item_t* cbor = nm.to_cbor();
  std::stringstream debug_cbor;
  debug_cbor << cbor;
  cbor_decref(&cbor);

  BOOST_CHECK_EQUAL(
      "{\"codecName\":\"dummy-codec\",\"codecData\":\"ZHVtbXktY29kZWMtZGF0YQ==\",\"fps\":"
      "25}",
      debug_cbor.str());
}
