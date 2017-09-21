#include <boost/bind.hpp>
#include <iostream>

#include "base64.h"
#include "error.h"
#include "logging.h"
#include "video_streams.h"

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
}

namespace rtm {
namespace video {

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
                 encoded_frame frame{.data = decode64(s->aggregated_data),
                                     .id = s->id,
                                     .timestamp = nf->t};
                 s->reset();
                 return streams::publishers::of({encoded_packet{frame}});
               }

               s->chunk++;
               return streams::publishers::empty<encoded_packet>();
             }

             BOOST_UNREACHABLE_RETURN();
           })
           >> streams::do_finally([s]() { delete s; });
  };
}

}  // namespace video
}  // namespace rtm
