#pragma once

#include <satorivideo/video_bot.h>
#include <opencv2/opencv.hpp>

namespace satori {
namespace video {

// Bot image callback receiving OpenCV Mat object.
// This cv::Mat object does not own frame data, and it is guaranteed to live
// only during callback execution frame. Perform deep clone
// if you need to use it later.
using opencv_bot_img_callback_t =
    std::function<void(bot_context &context, const cv::Mat &img)>;

struct opencv_bot_descriptor {
  // Invoked on every received image
  opencv_bot_img_callback_t img_callback;

  // Invoked on every received control command, guaranteed to be invoked during
  // initialization
  bot_ctrl_callback_t ctrl_callback;
};

// Registers opencv bot.
// Should be called by bot implementation before starting a bot.
EXPORT void opencv_bot_register(const opencv_bot_descriptor &bot);

// Starts a bot (e.g. launches main event loop).
// A bot implementation should be registered before calling this method.
EXPORT int opencv_bot_main(int argc, char *argv[]);
}  // namespace video
}  // namespace satori
