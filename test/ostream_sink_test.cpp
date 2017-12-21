#define BOOST_TEST_MODULE VP9EncoderTest
#include <boost/test/included/unit_test.hpp>

#include <sstream>
#include "ostream_sink.h"

namespace sv = satori::video;

BOOST_AUTO_TEST_CASE(basic) {
  std::ostringstream stream;
  auto &sink = sv::streams::ostream_sink(stream);
  sink.on_next(cbor_build_string("one"));
  sink.on_next(cbor_build_string("two"));
  sink.on_next(cbor_build_string("three"));
  sink.on_complete();

  BOOST_CHECK_EQUAL("\"one\"\n\"two\"\n\"three\"\n", stream.str());
}