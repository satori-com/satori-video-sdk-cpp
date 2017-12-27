#define BOOST_TEST_MODULE VP9EncoderTest
#include <boost/test/included/unit_test.hpp>

#include <sstream>
#include "ostream_sink.h"

namespace sv = satori::video;

BOOST_AUTO_TEST_CASE(basic) {
  std::ostringstream stream;
  auto &sink = sv::streams::ostream_sink(stream);
  sink.on_next(nlohmann::json("one"));
  sink.on_next(nlohmann::json("two"));
  sink.on_next(nlohmann::json("three"));
  sink.on_complete();

  BOOST_CHECK_EQUAL("\"one\"\n\"two\"\n\"three\"\n", stream.str());
}