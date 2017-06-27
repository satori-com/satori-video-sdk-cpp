#pragma once
#include <cbor.h>
#include <list>

enum class image_pixel_format { RGB0 = 1, BGR = 2 };

struct bot_message;

struct bot_context {
  std::list<bot_message> message_buffer;
};

constexpr char metadata_channel_suffix[] = "/metadata";
constexpr char analysis_channel_suffix[] = "/analysis";
constexpr char debug_channel_suffix[] =    "/debug";
