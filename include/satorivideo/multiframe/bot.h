// Multiframe bot API.
// This API is more lowlevel then main one and recommended to use if you need to control
// frame dropping.

#pragma once

#include <satorivideo/video_bot.h>
#include <gsl/span>
#include "../video_bot.h"

namespace satori {
namespace video {

using multiframe_bot_img_callback_t =
    std::function<void(bot_context &context, const gsl::span<image_frame> &frames)>;

struct multiframe_bot_descriptor {
  // Pixel format, like RGB0, BGR, etc.
  image_pixel_format pixel_format;

  // Invoked on every received image
  multiframe_bot_img_callback_t img_callback;

  // Invoked on every received control command, guaranteed to be invoked during
  // initialization
  bot_ctrl_callback_t ctrl_callback;
};

// Registers opencv bot.
// Should be called by bot implementation before starting a bot.
EXPORT void multiframe_bot_register(const multiframe_bot_descriptor &bot);

// Starts a bot (e.g. launches main event loop).
// A bot implementation should be registered before calling this method.
EXPORT int multiframe_bot_main(int argc, char *argv[]);
}  // namespace video
}  // namespace satori
