#pragma once

#include <satorivideo/video_bot.h>
#include <opencv2/opencv.hpp>

namespace satori {
namespace video {

//
// Image handler callback function that accepts an OpenCV `Mat` object.
// The object doesn't own the frame data it contains, and it's only valid
// until you return from the callback. To keep a persistent copy, do a deep clone
// and store the result in the bot_context.
//
using opencv_bot_img_callback_t =
    std::function<void(bot_context &context, const cv::Mat &img)>;

//
// Callback function that's invoked every time the bot framework finishes assembling an image
//
struct opencv_bot_descriptor {
  opencv_bot_img_callback_t img_callback;

//
// Callback function that's invoked every time the bot framework receives a message from the control
// channel
//
// The bot invokes this function at least once, during initialization
//
  bot_ctrl_callback_t ctrl_callback;
};

//
// Registers your bot
// Your code must call this function before calling `opencv_bot_main`.
//
EXPORT void opencv_bot_register(const opencv_bot_descriptor &bot);

//
// Starts your bot. The function starts the main event loop:
// - Retrieves incoming video frames
// - Passes the current frame to the video frame callback function
// - Passes the current frame to the opencv callback function
// Your code must call `opencv_bot_register` before calling this function
//
EXPORT int opencv_bot_main(int argc, char *argv[]);
}  // namespace video
}  // namespace satori
