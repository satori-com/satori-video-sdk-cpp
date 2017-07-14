#pragma once

#include <stddef.h>
#include <stdint.h>

#include "base.h"

struct video_source;

using metadata_handler = void (*)(const char *codec_name, size_t data_len, const uint8_t *data);
using frame_handler = void (*)(size_t data_len, const uint8_t *data);

EXPORT void video_source_init_library();

EXPORT video_source *video_source_file_new(const char *filename, int is_replayed);

EXPORT video_source *video_source_camera_new(const char *dimensions);

EXPORT void video_source_delete(video_source *video_source);

EXPORT void video_source_subscribe(video_source *video_source,
                                   metadata_handler metadata_handler,
                                   frame_handler frame_handler);

EXPORT void video_source_start(video_source *video_source);