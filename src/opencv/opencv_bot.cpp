#include "satorivideo/opencv/opencv_bot.h"
#include "../logging.h"
#include "satorivideo/video_bot.h"

namespace satori {
namespace video {

namespace {
cv::Mat get_image(const bot_context &context, const image_frame &frame) {
  CHECK(context.frame_metadata->width != 0);
  const uint8_t *buffer = frame.plane_data[0];
  auto line_size = context.frame_metadata->plane_strides[0];
  return cv::Mat(context.frame_metadata->height, context.frame_metadata->width, CV_8UC3,
                 (void *)buffer, line_size);
}

bot_img_callback_t to_bot_img_callback(const opencv_bot_img_callback_t &callback) {
  return [callback](bot_context &context, const image_frame &frame) {
    return callback(context, get_image(context, frame));
  };
}

}  // namespace

void opencv_bot_register(const opencv_bot_descriptor &bot) {
  bot_register({image_pixel_format::BGR, to_bot_img_callback(bot.img_callback),
                bot.ctrl_callback});
}

int opencv_bot_main(int argc, char **argv) { return bot_main(argc, argv); }

}  // namespace video
}  // namespace satori