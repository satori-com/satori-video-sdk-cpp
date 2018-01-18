#define BOOST_TEST_MODULE DataTest
#include <boost/test/included/unit_test.hpp>

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
  em.additional_data = nlohmann::json::object();
  em.additional_data["fps"] = 25;

  const sv::network_metadata& nm = em.to_network();
  nlohmann::json j = nm.to_json();

  nlohmann::json expected_j =
      R"({"codecName":"dummy-codec", "codecData":"ZHVtbXktY29kZWMtZGF0YQ==", "fps": 25})"_json;

  BOOST_CHECK_EQUAL(expected_j, j);
}

// TODO: remove after full migration to base64 applied to chunks
BOOST_AUTO_TEST_CASE(parse_network_frame_d) {
  nlohmann::json item;
  item["i"] = {0, 0};
  item["d"] = "dummy";
  const sv::network_frame f = sv::parse_network_frame(item);
  BOOST_CHECK(!f.base64_applied_to_chunks);
}

BOOST_AUTO_TEST_CASE(parse_network_frame_b) {
  nlohmann::json item;
  item["i"] = {0, 0};
  item["b"] = "dummy";
  const sv::network_frame f = sv::parse_network_frame(item);
  BOOST_CHECK(f.base64_applied_to_chunks);
}