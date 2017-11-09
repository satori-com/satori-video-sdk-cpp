#define BOOST_TEST_MODULE AVUtilsTest
#define BOOST_TEST_NO_MAIN
#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/included/unit_test.hpp>

#include "avutils.h"
#include "logging_impl.h"

namespace satori {
namespace video {
BOOST_AUTO_TEST_CASE(av_error_messages) {
  BOOST_CHECK_EQUAL("Resource temporarily unavailable",
                    avutils::error_msg(AVERROR(EAGAIN)));
}

BOOST_AUTO_TEST_CASE(av_pixel_format) {
  BOOST_CHECK_EQUAL(AV_PIX_FMT_BGR24,
                    avutils::to_av_pixel_format(image_pixel_format::BGR));

  BOOST_CHECK_EQUAL(AV_PIX_FMT_RGB0,
                    avutils::to_av_pixel_format(image_pixel_format::RGB0));
}

BOOST_AUTO_TEST_CASE(pixel_image_format) {
  BOOST_CHECK_EQUAL((int)image_pixel_format::BGR,
                    (int)avutils::to_image_pixel_format(AV_PIX_FMT_BGR24));

  BOOST_CHECK_EQUAL((int)image_pixel_format::RGB0,
                    (int)avutils::to_image_pixel_format(AV_PIX_FMT_RGB0));
}

BOOST_AUTO_TEST_CASE(encoder_context) {
  const AVCodecID encoder_id = AV_CODEC_ID_VP9;
  const AVCodec *encoder = avcodec_find_encoder(encoder_id);
  BOOST_TEST(encoder != nullptr);

  std::shared_ptr<AVCodecContext> ctx = avutils::encoder_context(encoder_id);
  BOOST_CHECK_EQUAL(AVMEDIA_TYPE_VIDEO, ctx->codec_type);
  BOOST_CHECK_EQUAL(encoder_id, ctx->codec_id);
  BOOST_CHECK_EQUAL(encoder->pix_fmts[0], ctx->pix_fmt);
  BOOST_CHECK_EQUAL(12, ctx->gop_size);
  BOOST_CHECK_EQUAL(1, ctx->time_base.num);
  BOOST_CHECK_EQUAL(1000, ctx->time_base.den);
}

BOOST_AUTO_TEST_CASE(av_frame) {
  const int width = 100;
  const int height = 50;
  const int align = 32;
  const AVPixelFormat pixel_format = AV_PIX_FMT_BGR24;
  std::shared_ptr<AVFrame> frame = avutils::av_frame(width, height, align, pixel_format);

  BOOST_CHECK_EQUAL(width, frame->width);
  BOOST_CHECK_EQUAL(height, frame->height);
  BOOST_CHECK_EQUAL(pixel_format, frame->format);
  BOOST_CHECK_EQUAL(FFALIGN(width, align) * 3, frame->linesize[0]);
}

BOOST_AUTO_TEST_CASE(sws) {
  const int src_width = 100;
  const int src_height = 50;
  const int src_align = 32;
  const AVPixelFormat src_pixel_format = AV_PIX_FMT_BGR0;
  std::shared_ptr<AVFrame> src_frame =
      avutils::av_frame(src_width, src_height, src_align, src_pixel_format);

  const int dst_width = 100;
  const int dst_height = 50;
  const int dst_align = 32;
  const AVPixelFormat dst_pixel_format = AV_PIX_FMT_RGB0;
  std::shared_ptr<AVFrame> dst_frame =
      avutils::av_frame(dst_width, dst_height, dst_align, dst_pixel_format);

  std::shared_ptr<SwsContext> ctx = avutils::sws_context(src_frame, dst_frame);

  const uint8_t r = 10;
  const uint8_t g = 16;
  const uint8_t b = 19;

  // updating topleft pixel
  src_frame->data[0][0] = b;
  src_frame->data[0][1] = g;
  src_frame->data[0][2] = r;

  avutils::sws_scale(ctx, src_frame, dst_frame);

  BOOST_CHECK_EQUAL(r, dst_frame->data[0][0]);
  BOOST_CHECK_EQUAL(g, dst_frame->data[0][1]);
  BOOST_CHECK_EQUAL(b, dst_frame->data[0][2]);
}

BOOST_AUTO_TEST_CASE(format_context) {
  std::shared_ptr<AVFormatContext> ctx =
      avutils::output_format_context("matroska", "test.mkv", [](AVFormatContext *) {});

  BOOST_CHECK_EQUAL("test.mkv", ctx->filename);
  BOOST_CHECK_EQUAL("Matroska", ctx->oformat->long_name);
}

BOOST_AUTO_TEST_CASE(copy_image_to_av_frame) {
  uint16_t width = 100;
  uint16_t height = 100;
  int align = 1;
  AVPixelFormat av_pixel_format = AV_PIX_FMT_RGB0;
  std::shared_ptr<AVFrame> frame =
      avutils::av_frame(width, height, align, av_pixel_format);

  owned_image_frame image;
  image.pixel_format = image_pixel_format::RGB0;
  image.width = width;
  image.height = height;
  unsigned int data_size = width * height * 3u;
  std::unique_ptr<uint8_t[]> data = std::make_unique<uint8_t[]>(data_size);
  for (size_t i = 0; i < data_size; i++) {
    data[i] = static_cast<uint8_t>((i * i) % 256);
  }
  image.plane_data[0].assign(data.get(), data.get() + data_size);
  image.plane_strides[0] = width * 3u;

  avutils::copy_image_to_av_frame(image, frame);

  BOOST_TEST(!memcmp(data.get(), frame->data[0], data_size));
}

BOOST_AUTO_TEST_CASE(parse_image_size) {
  boost::optional<image_size> s = avutils::parse_image_size("asdf");
  BOOST_TEST(!s);

  s = avutils::parse_image_size("137x245");
  BOOST_CHECK_EQUAL(137, s->width);
  BOOST_CHECK_EQUAL(245, s->height);

  s = avutils::parse_image_size("original");
  BOOST_CHECK_EQUAL(ORIGINAL_IMAGE_WIDTH, s->width);
  BOOST_CHECK_EQUAL(ORIGINAL_IMAGE_HEIGHT, s->height);
}

BOOST_AUTO_TEST_CASE(av_frame_to_image) {
  uint16_t width = 32;
  uint16_t height = 32;
  uint16_t rgb_components = 3;
  uint16_t stride = width * rgb_components;
  uint16_t data_size = width * height * rgb_components;

  std::shared_ptr<AVFrame> av_frame =
      avutils::av_frame(width, height, 1, AV_PIX_FMT_RGB0);

  av_frame->linesize[0] = stride;
  av_frame->data[0] = new uint8_t[data_size];
  av_frame->data[0][0] = 0xab;
  av_frame->data[0][data_size - 1] = 0xcd;

  owned_image_frame frame = avutils::to_image_frame(av_frame);

  BOOST_CHECK_EQUAL(width, frame.width);
  BOOST_CHECK_EQUAL(height, frame.height);
  BOOST_CHECK_EQUAL((int)image_pixel_format::RGB0, (int)frame.pixel_format);
  BOOST_CHECK_EQUAL(width * rgb_components, frame.plane_strides[0]);

  BOOST_CHECK_EQUAL(0, frame.plane_strides[1]);
  BOOST_CHECK_EQUAL(0, frame.plane_strides[2]);
  BOOST_CHECK_EQUAL(0, frame.plane_strides[3]);

  BOOST_CHECK_EQUAL(data_size, frame.plane_data[0].size());
  BOOST_CHECK_EQUAL(0, frame.plane_data[1].size());
  BOOST_CHECK_EQUAL(0, frame.plane_data[2].size());
  BOOST_CHECK_EQUAL(0, frame.plane_data[3].size());

  BOOST_CHECK_EQUAL(0xab, (uint8_t)frame.plane_data[0][0]);
  BOOST_CHECK_EQUAL(0xcd, (uint8_t)frame.plane_data[0][data_size - 1]);
}

}  // namespace video
}  // namespace satori

int main(int argc, char *argv[]) {
  init_logging(argc, argv);
  satori::video::avutils::init();
  return boost::unit_test::unit_test_main(init_unit_test, argc, argv);
}
