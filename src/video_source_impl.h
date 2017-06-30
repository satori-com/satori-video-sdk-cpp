#pragma once

#include "librtmvideo/video_source.h"

struct video_source {
 public:
  virtual ~video_source() = default;
  virtual int init() = 0;
  virtual int next_packet(uint8_t **output) = 0;
  virtual char *codec_name() const = 0;
  virtual int codec_data(uint8_t **output) const = 0;
  virtual size_t number_of_packets() const = 0;
  virtual double fps() const = 0;
};