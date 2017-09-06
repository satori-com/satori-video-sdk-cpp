#define BOOST_TEST_MODULE VP9TranscoderTest
#include <boost/test/included/unit_test.hpp>

#include "vp9_transcoder.h"

namespace {
struct dummy_sink
    : public rtm::video::sink<rtm::video::encoded_metadata, rtm::video::encoded_frame> {
 public:
  void on_metadata(rtm::video::encoded_metadata &&m) override { this->metadata = m; }
  void on_frame(rtm::video::encoded_frame &&f) override {
    frames.push_back(std::move(f));
  }

  bool empty() override { return true; }

  rtm::video::encoded_metadata metadata;
  std::vector<rtm::video::encoded_frame> frames;
};
}  // namespace

BOOST_AUTO_TEST_CASE(vp9_transcoder) {
  rtm::video::vp9_transcoder transcoder{1};
  std::shared_ptr<dummy_sink> s = std::make_shared<dummy_sink>();
  transcoder.subscribe(s);

  uint8_t pixel_data[] = {0xff, 0x88, 0x11};
  const int number_of_frames = 17;
  for (int i = 0; i < number_of_frames; i++) {
    rtm::video::image_frame f{
        .id = {i, i}, .pixel_format = image_pixel_format::RGB0, .width = 1, .height = 1,
    };
    f.plane_data[0] = std::string{pixel_data, pixel_data + sizeof(pixel_data)};
    f.plane_strides[0] = 3;

    transcoder.on_frame(std::move(f));
  }

  BOOST_CHECK_EQUAL("vp9", s->metadata.codec_name);
  BOOST_CHECK_EQUAL("", s->metadata.codec_data);
  BOOST_CHECK_EQUAL(number_of_frames, s->frames.size());
}
