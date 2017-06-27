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
int decoder_process_frame(decoder *d, const uint8_t *frame_data, size_t len);
bool decoder_frame_ready(decoder *d);

int decoder_image_height(decoder *d);
int decoder_image_width(decoder *d);
int decoder_image_line_size(decoder *d);
const uint8_t *decoder_image_data(decoder *d);
}