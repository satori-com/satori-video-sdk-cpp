#pragma once

#include <memory>

#include "librtmvideo/data.h"
#include "librtmvideo/decoder.h"
#include "sink.h"
#include "source.h"

namespace rtm {
namespace video {

struct flow_decoder : public sink<metadata, encoded_frame>,
                      public source<metadata, image_frame> {
 public:
  flow_decoder(uint16_t width, uint16_t height,
               image_pixel_format pixel_format);
  ~flow_decoder();

  int init() override;
  void start() override;
  void on_metadata(metadata &&m) override;
  void on_frame(encoded_frame &&f) override;

 private:
  const uint16_t _width;
  const uint16_t _height;
  const image_pixel_format _pixel_format;
  std::shared_ptr<decoder> _decoder;
  metadata _metadata;
};

}  // namespace video
}  // namespace rtm
