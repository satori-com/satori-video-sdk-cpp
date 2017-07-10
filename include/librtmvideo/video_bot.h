#pragma once

#include <cbor.h>
#include <stdint.h>

#include "base.h"
#include "rtmvideo.h"

// Video bot API.
EXPORT struct bot_context {
  // bot implementation can store state in the field.
  void *instance_data{nullptr};
};

using bot_img_callback_t = void (*)(bot_context &context, const uint8_t *image,
                                    uint16_t width, uint16_t height,
                                    uint16_t linesize);

using bot_ctrl_callback_t = int (*)(bot_context &context, cbor_item_t *message);

struct bot_descriptor {
  uint16_t image_width;
  uint16_t image_height;
  image_pixel_format pixel_format;

  // Image callback is invoked on every received frame
  bot_img_callback_t img_callback;

  // Command callback is invoked before the first frame with a map
  // {"action": "configure", "body":{<configure_parameters if specified>}}
  bot_ctrl_callback_t ctrl_callback;
};

EXPORT enum class bot_message_kind { ANALYSIS = 1, DEBUG = 2 };

EXPORT void rtm_video_bot_message(bot_context &context,
                                  const bot_message_kind kind,
                                  cbor_item_t *message);
EXPORT void rtm_video_bot_register(const bot_descriptor &bot);
EXPORT int rtm_video_bot_main(int argc, char *argv[]);
