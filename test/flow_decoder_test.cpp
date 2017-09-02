#define BOOST_TEST_MODULE FlowDecoderTest
#include <boost/test/included/unit_test.hpp>

#include <fstream>
#include "base64.h"
#include "flow_decoder.h"
#include "librtmvideo/data.h"

namespace {
struct dummy_sink
    : public rtm::video::sink<rtm::video::image_metadata, rtm::video::image_frame> {
 public:
  void on_metadata(rtm::video::image_metadata &&m) override {}
  void on_frame(rtm::video::image_frame &&f) override {
    last_frame_width = f.width;
    last_frame_height = f.height;
    frames_count++;
  }

  bool empty() override { return true; }

  int last_frame_width;
  int last_frame_height;
  int frames_count{0};
};

struct test_definition {
  std::string metadata_filename;
  std::string frames_filename;
  std::string codec_name;
  int expected_width;
  int expected_height;
  int expected_frames_count;
};

void run_flow_decoder_test(test_definition &&td) {
  std::cout << "*** running test for codec '" << td.codec_name << "'\n";
  std::ifstream metadata_file(td.metadata_filename);
  std::ifstream frame_file(td.frames_filename);
  if (td.metadata_filename != "" && !metadata_file)
    BOOST_FAIL("File '" + td.metadata_filename + "' was not found");
  if (!frame_file) BOOST_FAIL("File '" + td.frames_filename + "' was not found");

  std::string base64_metadata;
  metadata_file >> base64_metadata;

  rtm::video::flow_decoder d{-1, -1, image_pixel_format::RGB0};
  std::shared_ptr<dummy_sink> s = std::make_shared<dummy_sink>();
  d.subscribe(s);

  d.on_metadata({.codec_name = td.codec_name,
                 .codec_data = rtm::video::decode64(base64_metadata)});

  std::string base64_data;
  while (std::getline(frame_file, base64_data)) {
    d.on_frame({.data = rtm::video::decode64(base64_data), .id = {-1, -1}});
  }

  BOOST_TEST(td.expected_width == s->last_frame_width);
  BOOST_TEST(td.expected_height == s->last_frame_height);
  BOOST_TEST(td.expected_frames_count == s->frames_count);
  std::cout << "*** test for codec '" << td.codec_name << "' succeeded\n";
}

}  // namespace

BOOST_AUTO_TEST_CASE(vp9) {
  run_flow_decoder_test({
    .metadata_filename = "",
    .frames_filename = "test_data/vp9_320x180.frame",
    .codec_name = "vp9",
    .expected_width = 320,
    .expected_height = 180,
    .expected_frames_count = 1,
  });
}

BOOST_AUTO_TEST_CASE(h264) {
  run_flow_decoder_test({
    .metadata_filename = "test_data/h264_320x180.metadata",
    .frames_filename =  "test_data/h264_320x180.frame",
    .codec_name = "h264",
    .expected_width = 320,
    .expected_height = 180,
    .expected_frames_count = 1,
  });
}

BOOST_AUTO_TEST_CASE(jpeg) {
  run_flow_decoder_test({
    .metadata_filename = "",
    .frames_filename = "test_data/jpeg_320x180.frame",
    .codec_name = "mjpeg",
    .expected_width = 320,
    .expected_height = 180,
    .expected_frames_count = 1,
  });
}

BOOST_AUTO_TEST_CASE(mjpeg) {
  run_flow_decoder_test({
    .metadata_filename = "",
    .frames_filename = "test_data/mjpeg_320x180.frame",
    .codec_name = "mjpeg",
    .expected_width = 320,
    .expected_height = 180,
    .expected_frames_count = 1,
  });
}
