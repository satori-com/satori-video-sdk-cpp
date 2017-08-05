#include <boost/assert.hpp>
#include <iostream>

#include "flow_decoder.h"
#include "stopwatch.h"
#include "tele_impl.h"

namespace {
auto frames_received = tele::counter_new("decoder", "frames_received");
auto messages_received = tele::counter_new("decoder", "messages_received");
auto bytes_received = tele::counter_new("decoder", "bytes_received");
auto decoding_times_millis =
    tele::distribution_new("decoder", "decoding_times_millis");
}  // namespace

namespace rtm {
namespace video {

flow_decoder::flow_decoder(uint16_t width, uint16_t height,
                           image_pixel_format pixel_format)
    : _width(width), _height(height), _pixel_format(pixel_format) {
  static bool library_initialized = false;
  if (!library_initialized) {
    decoder_init_library();
    library_initialized = true;
  }

  _decoder.reset(decoder_new_keep_proportions(_width, _height, _pixel_format),
                 [this](decoder *d) {
                   std::cout << "Deleting decoder\n";
                   decoder_delete(d);
                 });
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

  _decoder.reset(decoder_new_keep_proportions(_width, _height, _pixel_format),
                 [this](decoder *d) {
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

  if (!_decoder) {
    return;
  }

  tele::counter_inc(bytes_received, f.data.size());

  decoder *decoder = _decoder.get();
  {
    stopwatch<> s;
    decoder_process_binary_message(decoder, (const uint8_t *)f.data.data(),
                                   f.data.size());
    tele::distribution_add(decoding_times_millis, s.millis());
  }

  if (decoder_frame_ready(decoder)) {
    tele::counter_inc(frames_received);

    const uint16_t width = decoder_image_width(decoder);
    const uint16_t height = decoder_image_height(decoder);
    const uint16_t linesize = decoder_image_line_size(decoder);
    const std::string image_data{(const char *)decoder_image_data(decoder),
                                 (size_t)height * linesize};

    source::foreach_sink([&image_data, &f, width, height, linesize](auto s) {
      s->on_frame({.image_data = image_data,
                   .id = f.id,
                   .width = width,
                   .height = height,
                   .linesize = linesize});
    });
  }
}
}  // namespace video
}  // namespace rtm
