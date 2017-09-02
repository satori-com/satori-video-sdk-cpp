#include <iostream>

#include "flow_decoder.h"
#include "stopwatch.h"
#include "tele_impl.h"

namespace {
auto frames_received = tele::counter_new("decoder", "frames_received");
auto messages_received = tele::counter_new("decoder", "messages_received");
auto messages_dropped = tele::counter_new("decoder", "messages_dropped");
auto bytes_received = tele::counter_new("decoder", "bytes_received");
auto decoding_times_millis =
    tele::distribution_new("decoder", "decoding_times_millis");
}  // namespace

namespace rtm {
namespace video {

flow_decoder::flow_decoder(int bounding_width, int bounding_height,
                           image_pixel_format pixel_format)
    : _bounding_width(bounding_width),
      _bounding_height(bounding_height),
      _pixel_format(pixel_format) {
  static bool library_initialized = false;
  if (!library_initialized) {
    decoder_init_library();
    library_initialized = true;
  }
}

flow_decoder::~flow_decoder() {}

int flow_decoder::init() { return 0; }

void flow_decoder::start() {}

void flow_decoder::on_metadata(metadata &&m) {
  if (m.codec_data == _metadata.codec_data &&
      m.codec_name == _metadata.codec_name) {
    return;
  }

  _metadata = m;

  _decoder.reset(decoder_new_keep_proportions(_bounding_width, _bounding_height,
                                              _pixel_format),
                 [](decoder *d) {
                   std::cout << "Deleting decoder\n";
                   decoder_delete(d);
                 });
  BOOST_VERIFY(_decoder);

  decoder_set_metadata(_decoder.get(), _metadata.codec_name.c_str(),
                       (const uint8_t *)_metadata.codec_data.data(),
                       _metadata.codec_data.size());
  std::cout << "Video decoder initialized\n";
}

void flow_decoder::on_frame(encoded_frame &&f) {
  tele::counter_inc(messages_received);
  tele::counter_inc(bytes_received, f.data.size());

  if (!_decoder) {
    tele::counter_inc(messages_dropped);
    return;
  }

  decoder *decoder = _decoder.get();
  {
    stopwatch<> s;
    decoder_process_binary_message(decoder, (const uint8_t *)f.data.data(),
                                   f.data.size());
    tele::distribution_add(decoding_times_millis, s.millis());
  }

  if (!decoder_frame_ready(decoder)) return;

  tele::counter_inc(frames_received);
  uint16_t width = (uint16_t)decoder_image_width(decoder);
  uint16_t height = (uint16_t)decoder_image_height(decoder);

  source::foreach_sink([this, &f, decoder, width, height](auto s) {
    image_frame frame{.id = f.id,
                      .pixel_format = _pixel_format,
                      .width = width,
                      .height = height};

    for (uint8_t i = 0; i < MAX_IMAGE_PLANES; i++) {
      const uint32_t plane_stride = decoder_image_line_size(decoder, i);
      const uint8_t *plane_data = decoder_image_data(decoder, i);
      frame.plane_strides[i] = plane_stride;
      if (plane_stride > 0) {
        frame.plane_data[i].assign(plane_data,
                                    plane_data + (plane_stride * height));
      }
    }

    s->on_frame(std::move(frame));
  });
}

bool flow_decoder::empty() { return true; }

}  // namespace video
}  // namespace rtm