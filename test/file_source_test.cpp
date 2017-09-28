#define BOOST_TEST_MODULE FileSourceTest
#include <boost/test/included/unit_test.hpp>

#include "librtmvideo/data.h"
#include "video_streams.h"

using namespace rtm::video;

namespace {

inline frame_id id(int64_t i1, int64_t i2) { return frame_id{i1, i2}; }

}  // namespace

BOOST_AUTO_TEST_CASE(test_frame_ids) {
  boost::asio::io_service io;

  std::vector<frame_id> ids;
  file_source(io, "test_data/test.mp4", false, true)
      ->process(
          [&ids](encoded_packet &&pkt) {
            if (const encoded_frame *f = boost::get<encoded_frame>(&pkt)) {
              ids.push_back(f->id);
            }
          },
          []() {}, [](std::error_condition ec) { BOOST_TEST(false); });

  BOOST_TEST(ids.size() == 6);
  BOOST_TEST(ids[0] == id(0, 48));
  BOOST_TEST(ids[1] == id(49, 28975));
  BOOST_TEST(ids[2] == id(28976, 32918));
  BOOST_TEST(ids[3] == id(32919, 38321));
  BOOST_TEST(ids[4] == id(38322, 44809));
  BOOST_TEST(ids[5] == id(44810, 47582));
}