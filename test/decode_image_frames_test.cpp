#define BOOST_TEST_MODULE DecodeImageFrames
#define BOOST_TEST_NO_MAIN
#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/included/unit_test.hpp>

#include <fstream>
#include "avutils.h"
#include "base64.h"
#include "data.h"
#include "logging_impl.h"
#include "video_streams.h"

namespace sv = satori::video;

namespace {

struct test_definition {
  std::string metadata_filename;
  std::string frames_filename;
  std::string codec_name;
  int expected_width;
  int expected_height;
  int expected_frames_count;
};

sv::streams::publisher<sv::encoded_packet> test_stream(const test_definition &td) {
  std::ifstream metadata_file(td.metadata_filename);
  if (!td.metadata_filename.empty() && !metadata_file) {
    BOOST_FAIL("File '" + td.metadata_filename + "' was not found");
  }

  std::string base64_metadata;
  metadata_file >> base64_metadata;

  sv::encoded_packet metadata(
      sv::encoded_metadata{td.codec_name, sv::base64::decode(base64_metadata)});

  sv::streams::publisher<sv::encoded_packet> frames =
      sv::streams::read_lines(td.frames_filename)
      >> sv::streams::map([](std::string &&line) {
          static int next_id = 0;
          int id = ++next_id;

          sv::encoded_frame f;
          f.data = sv::base64::decode(line);
          f.id = sv::frame_id{id, id};
          return sv::encoded_packet{f};
        });

  return sv::streams::publishers::concat(sv::streams::publishers::of({metadata}),
                                         std::move(frames));
}

void run_decode_image_frames_test(const test_definition &td) {
  std::cout << "*** running test for codec '" << td.codec_name << "'\n";
  std::ifstream frame_file(td.frames_filename);

  if (!frame_file) {
    BOOST_FAIL("File '" + td.frames_filename + "' was not found");
  }

  sv::streams::publisher<sv::owned_image_packet> image_stream =
      test_stream(td)
      >> sv::decode_image_frames(-1, -1, sv::image_pixel_format::RGB0, true);

  int last_frame_width;
  int last_frame_height;
  int frames_count{0};

  auto when_done =
      image_stream->process([&last_frame_width, &last_frame_height,
                             &frames_count](sv::owned_image_packet &&packet) mutable {
        if (const sv::owned_image_metadata *m =
                boost::get<sv::owned_image_metadata>(&packet)) {
          // just count
        } else if (const sv::owned_image_frame *f =
                       boost::get<sv::owned_image_frame>(&packet)) {
          last_frame_width = f->width;
          last_frame_height = f->height;
        } else {
          BOOST_FAIL("Bad variant");
        }
        frames_count++;
      });

  BOOST_TEST(when_done.ok());

  BOOST_TEST(td.expected_width == last_frame_width);
  BOOST_TEST(td.expected_height == last_frame_height);
  BOOST_TEST(td.expected_frames_count == frames_count);
  std::cout << "*** test for codec '" << td.codec_name << "' succeeded\n";
}

inline sv::frame_id id(int64_t i1, int64_t i2) { return sv::frame_id{i1, i2}; }

}  // namespace

BOOST_AUTO_TEST_CASE(vp9) {
  test_definition test;

  test.metadata_filename = "";
  test.frames_filename = "test_data/vp9_320x180.frame";
  test.codec_name = "vp9";
  test.expected_width = 320;
  test.expected_height = 180;
  test.expected_frames_count = 1;

  run_decode_image_frames_test(test);
}

BOOST_AUTO_TEST_CASE(h264) {
  test_definition test;

  test.metadata_filename = "test_data/h264_320x180.metadata";
  test.frames_filename = "test_data/h264_320x180.frame";
  test.codec_name = "h264";
  test.expected_width = 320;
  test.expected_height = 180;
  test.expected_frames_count = 6;

  run_decode_image_frames_test(test);
}

BOOST_AUTO_TEST_CASE(jpeg) {
  test_definition test;

  test.metadata_filename = "";
  test.frames_filename = "test_data/jpeg_320x180.frame";
  test.codec_name = "mjpeg";
  test.expected_width = 320;
  test.expected_height = 180;
  test.expected_frames_count = 1;

  run_decode_image_frames_test(test);
}

BOOST_AUTO_TEST_CASE(mjpeg) {
  test_definition test;

  test.metadata_filename = "";
  test.frames_filename = "test_data/mjpeg_320x180.frame";
  test.codec_name = "mjpeg";
  test.expected_width = 320;
  test.expected_height = 180;
  test.expected_frames_count = 1;

  run_decode_image_frames_test(test);
}

BOOST_AUTO_TEST_CASE(id_test) {
  LOG_SCOPE_FUNCTION(INFO);

  boost::asio::io_service io;

  std::vector<sv::frame_id> ids;
  auto stream = sv::file_source(io, "test_data/test.mp4", false, true)
                >> sv::decode_image_frames(-1, -1, sv::image_pixel_format::RGB0, true);

  auto when_done = stream->process([&ids](sv::owned_image_packet &&pkt) {
    if (const sv::owned_image_frame *f = boost::get<sv::owned_image_frame>(&pkt)) {
      ids.push_back(f->id);
    }
  });
  BOOST_TEST(when_done.ok());

  BOOST_TEST(ids.size() == 6);
  BOOST_TEST(ids[0] == id(0, 48));
  BOOST_TEST(ids[1] == id(49, 28975));
  BOOST_TEST(ids[2] == id(28976, 32918));
  BOOST_TEST(ids[3] == id(38322, 44809));
  BOOST_TEST(ids[4] == id(44810, 47582));
  BOOST_TEST(ids[5] == id(32919, 38321));
}

int main(int argc, char *argv[]) {
  sv::init_logging(argc, argv);
  return boost::unit_test::unit_test_main(init_unit_test, argc, argv);
}