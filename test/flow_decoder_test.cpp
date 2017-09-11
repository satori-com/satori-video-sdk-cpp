#define BOOST_TEST_MODULE FlowDecoderTest
#include <boost/test/included/unit_test.hpp>

#include <fstream>
#include "base64.h"
#include "librtmvideo/data.h"
#include "video_streams.h"

using namespace rtm::video;

namespace {

struct test_definition {
  std::string metadata_filename;
  std::string frames_filename;
  std::string codec_name;
  int expected_width;
  int expected_height;
  int expected_frames_count;
};

streams::publisher<encoded_packet> test_stream(const test_definition &td) {
  std::ifstream metadata_file(td.metadata_filename);
  if (td.metadata_filename != "" && !metadata_file)
    BOOST_FAIL("File '" + td.metadata_filename + "' was not found");

  std::string base64_metadata;
  metadata_file >> base64_metadata;

  encoded_packet metadata(encoded_metadata{
      .codec_name = td.codec_name, .codec_data = rtm::video::decode64(base64_metadata)});

  streams::publisher<encoded_packet> frames =
      streams::read_lines(td.frames_filename) >> streams::map([](std::string &&line) {
        static int next_id = 0;
        int id = ++next_id;
        return encoded_packet{
            encoded_frame{.data = rtm::video::decode64(line), .id = {id, id}}};
      });

  return streams::publishers::merge(streams::publishers::of({metadata}),
                                    std::move(frames));
}

void run_flow_decoder_test(test_definition &&td) {
  std::cout << "*** running test for codec '" << td.codec_name << "'\n";
  initialize_source_library();

  std::ifstream frame_file(td.frames_filename);

  if (!frame_file) BOOST_FAIL("File '" + td.frames_filename + "' was not found");

  streams::publisher<owned_image_frame> image_stream =
      test_stream(td) >> lift(decode_image_frames(-1, -1, image_pixel_format::RGB0));

  int last_frame_width;
  int last_frame_height;
  int frames_count{0};

  image_stream->process(
      [&last_frame_width, &last_frame_height, &frames_count](owned_image_frame &&f) {
        last_frame_width = f.width;
        last_frame_height = f.height;
        frames_count++;

      },
      []() {}, [](const std::error_condition ec) { BOOST_FAIL(ec.message()); });

  BOOST_TEST(td.expected_width == last_frame_width);
  BOOST_TEST(td.expected_height == last_frame_height);
  BOOST_TEST(td.expected_frames_count == frames_count);
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
