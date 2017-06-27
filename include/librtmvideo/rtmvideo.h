#pragma once
#include <cbor.h>

enum class image_pixel_format { RGB0 = 1, BGR = 2 };

enum class bot_message_kind { ANALYSIS = 1, DEBUG = 2 };

struct bot_context {
  struct message_list *message_buffer;
};

struct message_list {
  cbor_item_t *data;
  bot_message_kind kind;
  struct message_list *next;
};

constexpr char metadata_channel_suffix[] = "/metadata";
constexpr char analysis_channel_suffix[] = "/analysis";
constexpr char debug_channel_suffix[] =    "/debug";
