#define BOOST_TEST_MODULE VP9EncoderTest
#include <boost/test/included/unit_test.hpp>

#include "logging.h"
#include "vp9_encoder.h"

using namespace rtm::video;

BOOST_AUTO_TEST_CASE(vp9_encoder) {
  const int gop_size =
      12;  // TODO: update this field after codec parameters are made configurable
  const int number_of_frames = 329;
  const int min_key_frames_count =
      (number_of_frames / gop_size) + (number_of_frames % gop_size ? 1 : 0);
  auto frames =
      streams::publishers::range(0, number_of_frames) >> streams::map([](int i) {
        uint8_t pixel_data[] = {0xff, 0x88, 0x11};
        owned_image_frame f;
        f.id = {i, i};
        f.pixel_format = image_pixel_format::RGB0;
        f.width = 1;
        f.height = 1;
        f.plane_data[0] = std::string{pixel_data, pixel_data + sizeof(pixel_data)};
        f.plane_strides[0] = 3;
        return owned_image_packet{f};
      });

  int packets_count{0};
  int key_frames_count{0};
  int frames_from_last_key_frame_count{0};
  auto encoded_stream = std::move(frames) >> encode_vp9(1);
  encoded_stream->process(
      [&packets_count, &key_frames_count, &frames_from_last_key_frame_count,
       gop_size](encoded_packet &&packet) mutable {
        if (const encoded_frame *f = boost::get<encoded_frame>(&packet)) {
          if (f->key_frame) {
            BOOST_TEST(frames_from_last_key_frame_count <= gop_size);
            frames_from_last_key_frame_count = 0;
            key_frames_count++;
          }
        }
        packets_count++;
      },
      []() {}, [](const std::error_condition ec) { BOOST_FAIL(ec.message()); });

  BOOST_CHECK_EQUAL(number_of_frames + 1 /* metadata frame */, packets_count);

  LOG_S(INFO) << "min_key_frames_count = " << min_key_frames_count
              << ", key_frames_count = " << key_frames_count;
  BOOST_TEST(min_key_frames_count <= key_frames_count);
}
