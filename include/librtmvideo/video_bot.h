#pragma once
#include <stdint.h>
#include <cbor.h>

#include "rtmvideo.h"

// Video bot API.
extern "C" {
using bot_callback_t = void (*)(const uint8_t *image, uint16_t width,
                                uint16_t height, uint16_t linesize, struct bot_context *context);

enum PixelFormat { BGR24, RGB0 };

struct bot_descriptor {
  uint16_t image_width;
  uint16_t image_height;
  image_pixel_format pixel_format;
  bot_callback_t callback;
};

struct bot_context {
  struct message_list *message_buffer;
};

struct message_list {
  cbor_item_t *data;
  struct message_list *next;
};

void rtm_video_bot_message(cbor_item_t *data, struct bot_context *context);
void rtm_video_bot_register(const bot_descriptor &bot);
int rtm_video_bot_main(int argc, char *argv[]);
}
