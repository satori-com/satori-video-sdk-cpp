#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/accumulators/statistics/rolling_window.hpp>
#include <iostream>

#include "base64.h"
#include "logging.h"
#include "metrics.h"
#include "statsutils.h"
#include "video_error.h"
#include "video_streams.h"

namespace satori {
namespace video {

namespace {

constexpr std::initializer_list<double> time_delta_buckets = {
    0,    1,  2,    3,  4,  5,  6,  7,  8,  9,   10,  15,  20,  25,  30,  35,  39,
    39.9, 40, 40.1, 41, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 750, 1000};

auto &frame_chunks_mismatch = prometheus::BuildCounter()
                                  .Name("network_decoder_frame_chunks_mismatch")
                                  .Register(metrics_registry())
                                  .Add({});

auto &frame_id_deltas =
    prometheus::BuildHistogram()
        .Name("frame_id_deltas")
        .Register(metrics_registry())
        .Add({}, std::vector<double>{0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9,
                                     1, 2,   3,   4,   5,   6,   7,   8,   9,   10});

auto &frame_time_delta_millis = prometheus::BuildHistogram()
                                    .Name("frame_time_delta_millis")
                                    .Register(metrics_registry())
                                    .Add({}, std::vector<double>(time_delta_buckets));

auto &frame_departure_time_delta_millis =
    prometheus::BuildHistogram()
        .Name("frame_departure_time_delta_millis")
        .Register(metrics_registry())
        .Add({}, std::vector<double>(time_delta_buckets));

auto &frame_departure_time_jitter =
    prometheus::BuildHistogram()
        .Name("frame_departure_time_jitter")
        .Register(metrics_registry())
        .Add({}, std::vector<double>{0,  1,   2,   3,   4,   5,   6,   7,  8,  9,
                                     10, 15,  20,  25,  30,  40,  50,  60, 70, 80,
                                     90, 100, 150, 200, 250, 300, 400, 500});

auto &frame_arrival_time_delta_millis =
    prometheus::BuildHistogram()
        .Name("frame_arrival_time_delta_millis")
        .Register(metrics_registry())
        .Add({}, std::vector<double>(time_delta_buckets));

auto &frame_arrival_time_jitter =
    prometheus::BuildHistogram()
        .Name("frame_arrival_time_jitter")
        .Register(metrics_registry())
        .Add({}, std::vector<double>{0,  1,   2,   3,   4,   5,   6,   7,  8,  9,
                                     10, 15,  20,  25,  30,  40,  50,  60, 70, 80,
                                     90, 100, 150, 200, 250, 300, 400, 500});

auto &frame_delivery_delay_millis =
    prometheus::BuildHistogram()
        .Name("frame_delivery_delay_millis")
        .Register(metrics_registry())
        .Add({}, std::vector<double>{0,    1,     2,     3,     4,     5,    6,    7,
                                     8,    9,     10,    15,    20,    25,   30,   40,
                                     50,   60,    70,    80,    90,    100,  150,  200,
                                     250,  300,   400,   500,   600,   700,  800,  900,
                                     1000, 2000,  3000,  4000,  5000,  6000, 7000, 8000,
                                     9000, 10000, 15000, 20000, 30000, 60000});

auto &frame_chunks =
    prometheus::BuildHistogram()
        .Name("frame_chunks")
        .Register(metrics_registry())
        .Add({}, std::vector<double>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15, 20});

double observe_time_delta(const std::chrono::system_clock::time_point &t1,
                          const std::chrono::system_clock::time_point &t2,
                          prometheus::Histogram &histogram) {
  const double delta =
      std::abs(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t2).count());
  histogram.Observe(delta);
  return delta;
}

}  // namespace

streams::op<network_packet, encoded_packet> decode_network_stream() {
  struct packet_visitor : boost::static_visitor<streams::publisher<encoded_packet>> {
   public:
    streams::publisher<encoded_packet> operator()(const network_metadata &nm) {
      encoded_metadata em;
      em.codec_name = nm.codec_name;
      em.codec_data = decode64(nm.base64_data);
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

      _aggregated_data.append(nf.base64_data);
      if (nf.chunk == 1) {
        _id = nf.id;
        _timestamp = nf.t;
        _departure_time = nf.dt;
        _creation_time = nf.arrival_time;
      }

      if (nf.chunk == nf.chunks) {
        encoded_frame frame;
        frame.data = decode64(_aggregated_data);
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

streams::op<network_packet, network_packet> report_network_frame_dynamics() {
  struct packet_visitor : boost::static_visitor<void> {
   public:
    void operator()(const network_metadata & /*m*/) {}

    void operator()(const network_frame &f) {
      if (_first_frame) {
        _first_frame = false;
      } else {
        observe_time_delta(f.t, _last_time, frame_time_delta_millis);
        const double departure_time_delta = observe_time_delta(
            f.dt, _last_departure_time, frame_departure_time_delta_millis);
        const double arrival_time_delta = observe_time_delta(
            f.arrival_time, _last_arrival_time, frame_arrival_time_delta_millis);

        frame_delivery_delay_millis.Observe(
            std::abs(departure_time_delta - arrival_time_delta));

        _departure_time_jitter.emplace(departure_time_delta);
        _arrival_time_jitter.emplace(arrival_time_delta);

        frame_departure_time_jitter.Observe(_departure_time_jitter.value());
        frame_arrival_time_jitter.Observe(_arrival_time_jitter.value());
      }

      _last_time = f.t;
      _last_departure_time = f.dt;
      _last_arrival_time = f.arrival_time;
    }

   private:
    bool _first_frame{true};
    std::chrono::system_clock::time_point _last_time;
    std::chrono::system_clock::time_point _last_departure_time;
    std::chrono::system_clock::time_point _last_arrival_time;

    // TODO: prometheus can probably calculate standard deviation on it's own.
    statsutils::std_dev _departure_time_jitter{1000};
    statsutils::std_dev _arrival_time_jitter{1000};
  };

  return [](streams::publisher<network_packet> &&src) {
    packet_visitor visitor;
    return std::move(src)
           >> streams::map([visitor = std::move(visitor)](network_packet && p) mutable {
               boost::apply_visitor(visitor, p);
               return std::move(p);
             });
  };
}

streams::op<encoded_packet, encoded_packet> report_encoded_frame_dynamics() {
  struct packet_visitor : boost::static_visitor<void> {
   public:
    void operator()(const encoded_metadata & /*m*/) {}

    void operator()(const encoded_frame &f) {
      if (_first_frame) {
        _first_frame = false;
      } else {
        frame_id_deltas.Observe(std::abs(f.id.i1 - _last_id.i1));
      }

      _last_id = f.id;
    }

   private:
    bool _first_frame{true};
    frame_id _last_id;
  };

  return [](streams::publisher<encoded_packet> &&src) {
    packet_visitor visitor;
    return std::move(src)
           >> streams::map([visitor = std::move(visitor)](encoded_packet && p) mutable {
               boost::apply_visitor(visitor, p);
               return std::move(p);
             });
  };
}

}  // namespace video
}  // namespace satori
