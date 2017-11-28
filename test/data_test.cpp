#define BOOST_TEST_MODULE DataTest
#include <boost/test/included/unit_test.hpp>

#include "data.h"

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
