#include <boost/bind.hpp>
#include <iostream>

#include "base64.h"
#include "error.h"
#include "librtmvideo/decoder.h"
#include "librtmvideo/tele.h"
#include "stopwatch.h"
#include "video_streams.h"

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
}

namespace rtm {
namespace video {

void initialize_source_library() {
  static bool is_initialized = false;

  if (!is_initialized) {
    avdevice_register_all();
    avcodec_register_all();
    av_register_all();
    is_initialized = true;
  }
}

namespace {
auto frames_received = tele::counter_new("decoder", "frames_received");
auto messages_received = tele::counter_new("decoder", "messages_received");
auto messages_dropped = tele::counter_new("decoder", "messages_dropped");
auto bytes_received = tele::counter_new("decoder", "bytes_received");
auto decoding_times_millis = tele::distribution_new("decoder", "decoding_times_millis");
}  // namespace

streams::op<network_packet, encoded_packet> decode_network_stream() {
  struct state {
    uint8_t chunk{1};
    frame_id id;
    std::string aggregated_data;

    void reset() {
      chunk = 1;
      aggregated_data.clear();
    }
  };

  return [](streams::publisher<network_packet> &&src) {
    state *s = new state();
    return std::move(src) >> streams::flat_map([s](const network_packet &data) {
             if (const network_metadata *nm = boost::get<network_metadata>(&data)) {
               const std::string codec_data = decode64(nm->base64_data);
               return streams::publishers::of({encoded_packet(encoded_metadata{
                   .codec_name = nm->codec_name, .codec_data = codec_data})});
             } else if (const network_frame *nf = boost::get<network_frame>(&data)) {
               if (s->chunk != nf->chunk) {
                 s->reset();
                 return streams::publishers::empty<encoded_packet>();
               }

               s->aggregated_data.append(nf->base64_data);
               if (nf->chunk == 1) {
                 s->id = nf->id;
               }

               if (nf->chunk == nf->chunks) {
                 encoded_frame frame{.data = decode64(s->aggregated_data), .id = s->id};
                 s->reset();
                 return streams::publishers::of({encoded_packet{frame}});
               }

               s->chunk++;
               return streams::publishers::empty<encoded_packet>();
             }

             BOOST_UNREACHABLE_RETURN();
           }) >>
           streams::do_finally([s]() { delete s; });
  };
}
streams::op<encoded_packet, image_frame> decode_image_frames(
    int bounding_width, int bounding_height, image_pixel_format pixel_format) {
  struct state {
    state(int bounding_width, int bounding_height, image_pixel_format pixel_format)
        : _bounding_width(bounding_width),
          _bounding_height(bounding_height),
          _pixel_format(pixel_format) {}

    // returns 0 on success.
    streams::publisher<image_frame> on_metadata(const encoded_metadata &m) {
      if (m.codec_data == _metadata.codec_data && m.codec_name == _metadata.codec_name) {
        return streams::publishers::empty<image_frame>();
      }

      _metadata = m;

      _decoder.reset(
          decoder_new_keep_proportions(_bounding_width, _bounding_height, _pixel_format),
          [](decoder *d) {
            std::cout << "Deleting decoder\n";
            decoder_delete(d);
          });
      BOOST_VERIFY(_decoder);

      int err = decoder_set_metadata(_decoder.get(), _metadata.codec_name.c_str(),
                                     (const uint8_t *)_metadata.codec_data.data(),
                                     _metadata.codec_data.size());
      if (err)
        return streams::publishers::error<image_frame>(
            video_error::StreamInitializationError);
      std::cout << "Video decoder initialized\n";
      return streams::publishers::empty<image_frame>();
    }

    streams::publisher<image_frame> on_image_frame(const encoded_frame &f) {
      tele::counter_inc(messages_received);
      tele::counter_inc(bytes_received, f.data.size());

      if (!_decoder) {
        tele::counter_inc(messages_dropped);
        return streams::publishers::empty<image_frame>();
      }

      decoder *decoder = _decoder.get();
      {
        stopwatch<> s;
        decoder_process_binary_message(decoder, (const uint8_t *)f.data.data(),
                                       f.data.size());
        tele::distribution_add(decoding_times_millis, s.millis());
      }

      if (!decoder_frame_ready(decoder)) {
        return streams::publishers::empty<image_frame>();
      }

      tele::counter_inc(frames_received);

      auto width = (uint16_t)decoder_image_width(decoder);
      auto height = (uint16_t)decoder_image_height(decoder);

      image_frame frame{
          .id = f.id, .pixel_format = _pixel_format, .width = width, .height = height};

      for (uint8_t i = 0; i < MAX_IMAGE_PLANES; i++) {
        const uint32_t plane_stride = decoder_image_line_size(decoder, i);
        const uint8_t *plane_data = decoder_image_data(decoder, i);
        frame.plane_strides[i] = plane_stride;
        if (plane_stride > 0) {
          frame.plane_data[i].assign(plane_data, plane_data + (plane_stride * height));
        }
      }

      return streams::publishers::of({frame});
    }

   private:
    const int _bounding_width;
    const int _bounding_height;
    const image_pixel_format _pixel_format;
    std::shared_ptr<decoder> _decoder;
    encoded_metadata _metadata;
  };

  return [bounding_width, bounding_height,
          pixel_format](streams::publisher<encoded_packet> &&src) {
    state *s = new state(bounding_width, bounding_height, pixel_format);
    streams::publisher<image_frame> result =
        std::move(src) >> streams::flat_map([s](encoded_packet &&packet) {
          if (const encoded_metadata *m = boost::get<encoded_metadata>(&packet)) {
            return s->on_metadata(*m);
          } else if (const encoded_frame *f = boost::get<encoded_frame>(&packet)) {
            return s->on_image_frame(*f);
          } else {
            BOOST_ASSERT_MSG(false, "Bad variant");
          }
        }) >>
        streams::do_finally([s]() { delete s; });

    return result;
  };
}

}  // namespace video
}  // namespace rtm