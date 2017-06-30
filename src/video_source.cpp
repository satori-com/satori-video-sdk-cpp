#include "video_source_impl.h"

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
}

void video_source_init_library() {
  static bool is_initialized = false;

  if(!is_initialized) {
    avdevice_register_all();
    avcodec_register_all();
    av_register_all();
    is_initialized = true;
  }
}

void video_source_delete(video_source *video_source) {
  delete video_source;
}

size_t video_source_number_of_packets(video_source *video_source) {
  return video_source->number_of_packets();
}

double video_source_fps(video_source *video_source) {
  return video_source->fps();
}

int video_source_next_packet(video_source *video_source, uint8_t **output) {
  return video_source->next_packet(output);
}

char *video_source_codec_name(video_source *video_source) {
  return video_source->codec_name();
}

int video_source_codec_data(video_source *video_source, uint8_t **output) {
  return video_source->codec_data(output);
}