// Video bot API.
// There are two steps to define a video bot: register a bot and launch main
// event loop.
//
// Example:
// int main(int argc, char *argv[]) {
//   rtm_video_bot_register(bot_descriptor{640, 480, image_pixel_format::BGR,
//                                         &transcoder::process_image,
//                                         nullptr});
//   return rtm_video_bot_main(argc, argv);
// }

#pragma once

#include <cbor.h>
#include <stdint.h>

#include "base.h"
#include "rtmvideo.h"

// Used to store user defined state.
EXPORT struct bot_context { void *instance_data{nullptr}; };

// API for image handler callback
// An image may be split vertically into multiple lines, linesize defines how
// many lines are there. "width" is a width of an image, "height" is a height of
// a single line. In most cases there will be a single line.
using bot_img_callback_t = void (*)(bot_context &context, const uint8_t *image,
                                    uint16_t width, uint16_t height,
                                    uint16_t linesize);

// API for control command callback
// Format of message is defined by user.
// Recommended format is: {"action": "configure", "body":{<configure_parameters
// if specified>}}
using bot_ctrl_callback_t = cbor_item_t *(*)(bot_context &context,
                                             cbor_item_t *message);

struct bot_descriptor {
  // If received image's dimensions are greater than specified values,
  // then it will be automatically downscaled to provided values
  uint16_t image_width;
  uint16_t image_height;

  // Pixel format, like RGB0, BGR, etc.
  image_pixel_format pixel_format;

  // Invoked on every received image
  bot_img_callback_t img_callback;

  // Invoked on every received control command, guaranteed to be invoked during
  // initialization
  bot_ctrl_callback_t ctrl_callback;
};

// Used by bot implementation to specify type of output.
EXPORT enum class bot_message_kind { ANALYSIS = 1, DEBUG = 2 };

// Sends bot implementation output to RTM subchannel.
EXPORT void rtm_video_bot_message(bot_context &context,
                                  const bot_message_kind kind,
                                  cbor_item_t *message);

// Registers a bot.
// Should be called by bot implementation before starting a bot.
EXPORT void rtm_video_bot_register(const bot_descriptor &bot);

// Starts a bot (e.g. launches main event loop).
// A bot implementation should be registered before calling this method.
EXPORT int rtm_video_bot_main(int argc, char *argv[]);
