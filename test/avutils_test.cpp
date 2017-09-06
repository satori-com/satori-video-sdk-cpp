#define BOOST_TEST_MODULE AVUtilsTest
#include <boost/test/included/unit_test.hpp>

#include "avutils.h"

BOOST_AUTO_TEST_CASE(av_error_messages) {
  BOOST_CHECK_EQUAL("Resource temporarily unavailable",
                    rtm::video::avutils::error_msg(AVERROR(EAGAIN)));
}

BOOST_AUTO_TEST_CASE(av_pixel_format) {
  BOOST_CHECK_EQUAL(AV_PIX_FMT_BGR24, rtm::video::avutils::to_av_pixel_format(
                                          image_pixel_format::BGR));

  BOOST_CHECK_EQUAL(AV_PIX_FMT_RGB0, rtm::video::avutils::to_av_pixel_format(
                                         image_pixel_format::RGB0));
}