#pragma once
#include <cbor.h>
#include <list>

enum class image_pixel_format { RGB0 = 1, BGR = 2 };

constexpr char message_channel_suffix[] = "/control";
constexpr char metadata_channel_suffix[] = "/metadata";
constexpr char analysis_channel_suffix[] = "/analysis";
constexpr char debug_channel_suffix[] = "/debug";
