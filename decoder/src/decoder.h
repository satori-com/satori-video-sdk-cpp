#pragma once
#include <stddef.h>
#include <stdint.h>

#include "librtmvideo/base.h"
#include "librtmvideo/video_bot.h"

struct decoder;

EXPORT void decoder_init_library();

EXPORT decoder *decoder_new(int width, int height,
                            satori::video::image_pixel_format pixel_format);

EXPORT decoder *decoder_new_keep_proportions(
    int width, int height, satori::video::image_pixel_format pixel_format);

EXPORT void decoder_delete(decoder *d);

// Returns 0 if successful.
EXPORT int decoder_set_metadata(decoder *d, const char *codec_name,
                                const uint8_t *metadata, size_t len);

// Returns 0 if successful.
// For single-chunk messages chunk=1, chunks=1, frame_data is Base64 encoded
// frame. For multi-chunk messages all chunks of the same encoded frame should
// be concatenated before applying Base64 decoding.
EXPORT int decoder_process_frame_message(decoder *d, uint64_t i1, uint64_t i2,
                                         const uint8_t *frame_data, size_t len,
                                         uint32_t chunk, uint32_t chunks);

EXPORT bool decoder_frame_ready(decoder *d);

EXPORT int decoder_image_height(decoder *d);
EXPORT int decoder_image_width(decoder *d);

EXPORT int decoder_stream_height(decoder *d);
EXPORT int decoder_stream_width(decoder *d);
EXPORT double decoder_stream_fps(decoder *d);

EXPORT int decoder_image_line_size(decoder *d, uint8_t plane_index);
EXPORT int decoder_image_size(decoder *d);
EXPORT const uint8_t *decoder_image_data(decoder *d, uint8_t plane_index);
EXPORT uint64_t decoder_image_id1(decoder *d);
EXPORT uint64_t decoder_image_id2(decoder *d);
