#pragma once
#include <stddef.h>
#include <stdint.h>

#include "rtmvideo.h"

extern "C" {

struct decoder;

void decoder_init_library();

decoder *decoder_new(int width, int height, image_pixel_format pixel_format);
void decoder_delete(decoder *d);

// returns 0 if successful.
int decoder_set_metadata(decoder *d, const char *codec_name,
                         const uint8_t *metadata, size_t len);

// returns 0 if successful.
// for single-chunk messages chunk=1, chunks=1.
int decoder_process_frame_message(decoder *d, int64_t i1, int64_t i2,
                                  uint32_t rtp_timestamp, double ntp_timestamp,
                                  const uint8_t *frame_data, size_t len,
                                  int chunk, int chunks);
bool decoder_frame_ready(decoder *d);

int decoder_image_height(decoder *d);
int decoder_image_width(decoder *d);
int decoder_image_line_size(decoder *d);
const uint8_t *decoder_image_data(decoder *d);
}