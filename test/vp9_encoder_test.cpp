#define BOOST_TEST_MODULE VP9EncoderTest
#include <boost/test/included/unit_test.hpp>

#include "vp9_encoder.h"

using namespace rtm::video;

BOOST_AUTO_TEST_CASE(vp9_encoder) {
  const int number_of_frames = 17;
  auto frames =
      streams::publishers::range(0, number_of_frames) >> streams::map([](int i) {
        uint8_t pixel_data[] = {0xff, 0x88, 0x11};
        owned_image_frame f{
            .id = {i, i},
            .pixel_format = image_pixel_format::RGB0,
            .width = 1,
            .height = 1,
        };
        f.plane_data[0] = std::string{pixel_data, pixel_data + sizeof(pixel_data)};
        f.plane_strides[0] = 3;
        return owned_image_packet{f};
      });

  int frames_count{0};

  auto encoded_stream = std::move(frames) >> streams::lift(encode_vp9(1));
  encoded_stream->process(
      [&frames_count](encoded_packet &&f) { frames_count++; }, []() {},
      [](const std::error_condition ec) { BOOST_FAIL(ec.message()); });

  BOOST_CHECK_EQUAL(number_of_frames + 1 /* metadata frame */, frames_count);
}
