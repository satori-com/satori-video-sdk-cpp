#pragma once

#include <memory>

#include "librtmvideo/data.h"
#include "librtmvideo/decoder.h"
#include "sink.h"
#include "source.h"

namespace rtm {
namespace video {

// TODO: use streams API
struct flow_decoder : public sink<metadata, encoded_frame>,
                      public source<image_metadata, image_frame> {
 public:
  flow_decoder(int bounding_width, int bounding_height,
               image_pixel_format pixel_format);
  ~flow_decoder();

  int init() override;
  void start() override;
  void on_metadata(metadata &&m) override;
  void on_frame(encoded_frame &&f) override;
  bool empty() override;

 private:
  const int _bounding_width;
  const int _bounding_height;
  const image_pixel_format _pixel_format;
  std::shared_ptr<decoder> _decoder;
  metadata _metadata;
};

}  // namespace video
}  // namespace rtm