#pragma once

#include <stddef.h>
#include <stdint.h>

struct video_source;

void video_source_init_library();

video_source *video_source_file_new(const char *filename, int is_replayed);

video_source *video_source_camera_new(const char *dimensions);

void video_source_delete(video_source *video_source);

size_t video_source_number_of_packets(video_source *video_source);

double video_source_fps(video_source *video_source);

int video_source_next_packet(video_source *video_source, uint8_t **output);

char *video_source_codec_name(video_source *video_source);

int video_source_codec_data(video_source *video_source, uint8_t **output);