#define BOOST_TEST_MODULE AVFilterTest
#include <boost/test/included/unit_test.hpp>

#include "av_filter.h"
#include "avutils.h"

namespace sv = satori::video;

BOOST_AUTO_TEST_CASE(basic) {
  std::shared_ptr<AVFrame> in = sv::avutils::av_frame(64, 64, 1, AV_PIX_FMT_BGR0);
  in->sample_aspect_ratio.num = 1;
  in->sample_aspect_ratio.den = 1;
  sv::av_filter filter{"scale=w=16:h=16", *in, {1, 1}, sv::image_pixel_format::RGB0};
  filter.feed(*in);

  std::shared_ptr<AVFrame> out = sv::avutils::av_frame();
  BOOST_CHECK(filter.try_retrieve(*out));

  BOOST_CHECK(out->width == 16);
  BOOST_CHECK(out->height == 16);
  BOOST_CHECK(out->format == AV_PIX_FMT_RGB0);

  BOOST_CHECK(!filter.try_retrieve(*out));
}
