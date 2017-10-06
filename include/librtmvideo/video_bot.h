// Video bot API.
// There are two steps to define a video bot: register a bot and launch main
// event loop.
//
// Example:
// int main(int argc, char *argv[]) {
//   satori::video::bot_register({640, 480, image_pixel_format::BGR,
//                                         &transcoder::process_image,
//                                         nullptr});
//   return satori::video::bot_main(argc, argv);
// }

#pragma once

#include <stdint.h>

#include "base.h"

struct cbor_item_t;

namespace satori {
namespace video {

// Every image belongs to a certain time interval, setting values wider
// makes annotation applicable to multiple video frames
EXPORT struct frame_id {
  int64_t i1;
  int64_t i2;
};

constexpr uint8_t MAX_IMAGE_PLANES = 4;

// If an image uses packed pixel format like packed RGB or packed YUV,
// then it has only a single plane, e.g. all it's data is within plane_data[0].
// If an image uses planar pixel format like planar YUV or HSV,
// then every component is stored as a separate array (e.g. separate plane),
// for example, for YUV  Y is plane_data[0], U is plane_data[1] and V is
// plane_data[2]. A stride is a plane size with alignment.
EXPORT struct image_frame {
  frame_id id;
  const uint8_t *plane_data[MAX_IMAGE_PLANES];
};

// Metadata contains information which is unchangeable for a channel
EXPORT struct image_metadata {
  uint16_t width;
  uint16_t height;
  uint32_t plane_strides[MAX_IMAGE_PLANES];
};

// In batch mode, framework is waiting for each frame to be processed
// without drops to catch with live stream
EXPORT enum class execution_mode { LIVE = 1, BATCH = 2 };

EXPORT struct bot_context {
  // instance_data can be used to store data across multiple callbacks
  // within a single bot instance
  void *instance_data{nullptr};
  // image_metadata contains frame size information
  const image_metadata *frame_metadata;
  const execution_mode mode;
};

// API for image handler callback
using bot_img_callback_t = void (*)(bot_context &context, const image_frame &frame);

// API for control command callback
// Format of message is defined by user.
// Recommended format is: {"action": "configure", "body":{<configure_parameters
// if specified>}}
using bot_ctrl_callback_t = cbor_item_t *(*)(bot_context &context, cbor_item_t *message);

enum class image_pixel_format { RGB0 = 1, BGR = 2 };

struct bot_descriptor {
  // Pixel format, like RGB0, BGR, etc.
  image_pixel_format pixel_format;

  // Invoked on every received image
  bot_img_callback_t img_callback;

  // Invoked on every received control command, guaranteed to be invoked during
  // initialization
  bot_ctrl_callback_t ctrl_callback;
};

// Used by bot implementation to specify type of output.
EXPORT enum class bot_message_kind { ANALYSIS = 1, DEBUG = 2, CONTROL = 3 };

// Sends bot implementation output to RTM subchannel.
// id parameter is used to bind a message to a frame, by default a message is
// bound to the current frame that is received by the callback function
EXPORT void bot_message(bot_context &context, const bot_message_kind kind,
                        cbor_item_t *message, const frame_id &id = frame_id{0, 0});

// Registers a bot.
// Should be called by bot implementation before starting a bot.
EXPORT void bot_register(const bot_descriptor &bot);

// Starts a bot (e.g. launches main event loop).
// A bot implementation should be registered before calling this method.
EXPORT int bot_main(int argc, char *argv[]);

}  // namespace video
}  // namespace satori