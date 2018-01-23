#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/accumulators/statistics/rolling_window.hpp>
#include <iostream>

#include "base64.h"
#include "logging.h"
#include "metrics.h"
#include "video_error.h"
#include "video_streams.h"

namespace satori {
namespace video {

namespace {

auto &frame_chunks_mismatch = prometheus::BuildCounter()
                                  .Name("network_decoder_frame_chunks_mismatch")
                                  .Register(metrics_registry())
                                  .Add({});

auto &frame_chunks =
    prometheus::BuildHistogram()
        .Name("frame_chunks")
        .Register(metrics_registry())
        .Add({}, std::vector<double>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15, 20});

}  // namespace

streams::op<network_packet, encoded_packet> decode_network_stream() {
  struct packet_visitor : boost::static_visitor<streams::publisher<encoded_packet>> {
   public:
    streams::publisher<encoded_packet> operator()(const network_metadata &nm) {
      encoded_metadata em;
      em.codec_name = nm.codec_name;
      const auto data_or_error = base64::decode(nm.base64_data);
      CHECK(data_or_error.ok()) << "bad base64 data: " << nm.base64_data;
      em.codec_data = data_or_error.get();
      return streams::publishers::of({encoded_packet{em}});
    }

    streams::publisher<encoded_packet> operator()(const network_frame &nf) {
      if (_chunk != nf.chunk) {
        LOG(ERROR) << "chunk mismatch f.id=" << nf.id << " expected " << _chunk
                   << ", got " << nf.chunk;
        frame_chunks_mismatch.Increment();
        reset();
        return streams::publishers::empty<encoded_packet>();
      }

      if (nf.chunk == 1) {
        _id = nf.id;
        _timestamp = nf.t;
        _departure_time = nf.dt;
        _creation_time = nf.arrival_time;
        _base64_applied_to_chunks = nf.base64_applied_to_chunks;
      }

      if (_base64_applied_to_chunks) {
        const auto data_or_error = base64::decode(nf.base64_data);
        CHECK(data_or_error.ok()) << "bad base64 data: " << nf.base64_data;
        _aggregated_data.append(data_or_error.get());
      } else {
        _aggregated_data.append(nf.base64_data);
      }

      if (nf.chunk == nf.chunks) {
        encoded_frame frame;
        if (_base64_applied_to_chunks) {
          frame.data = _aggregated_data;
        } else {
          const auto data_or_error = base64::decode(_aggregated_data);
          CHECK(data_or_error.ok()) << "bad base64 data: " << _aggregated_data;
          frame.data = data_or_error.get();
        }
        frame.id = _id;
        frame.timestamp = _timestamp;
        frame.creation_time = _creation_time;
        reset();
        frame_chunks.Observe(nf.chunks);
        return streams::publishers::of({encoded_packet{frame}});
      }

      _chunk++;
      return streams::publishers::empty<encoded_packet>();
    }

   private:
    void reset() {
      _chunk = 1;
      _aggregated_data.clear();
    }

    uint8_t _chunk{1};
    frame_id _id;
    std::chrono::system_clock::time_point _timestamp;
    std::chrono::system_clock::time_point _departure_time;
    std::chrono::system_clock::time_point _creation_time;
    std::string _aggregated_data;
    // TODO: remove after full migration to base64 applied to chunks
    bool _base64_applied_to_chunks;
  };

  return [](streams::publisher<network_packet> &&src) {
    packet_visitor visitor;
    return std::move(src) >> streams::flat_map([visitor = std::move(visitor)](
                                 const network_packet &data) mutable {
             return boost::apply_visitor(visitor, data);
           });
  };
}

streams::op<encoded_packet, encoded_packet> repeat_metadata() {
  return streams::repeat_if<encoded_packet>(6000, [](const encoded_packet &p) {
    return nullptr != boost::get<encoded_metadata>(&p);
  });
}

}  // namespace video
}  // namespace satori
