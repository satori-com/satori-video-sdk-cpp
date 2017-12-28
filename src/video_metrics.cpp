#include "video_metrics.h"
#include "metrics.h"
#include "statsutils.h"

namespace satori {
namespace video {

namespace {

constexpr std::initializer_list<double> time_delta_buckets = {
    0,    1,  2,    3,  4,  5,  6,  7,  8,  9,   10,  15,  20,  25,  30,  35,  39,
    39.9, 40, 40.1, 41, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 750, 1000};

constexpr std::initializer_list<double> id_delta_buckets = {
    0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

constexpr std::initializer_list<double> jitter_buckets = {
    0,  1,  2,  3,  4,  5,  6,  7,   8,   9,   10,  15,  20,  25,
    30, 40, 50, 60, 70, 80, 90, 100, 150, 200, 250, 300, 400, 500};

constexpr std::initializer_list<double> delivery_delay_buckets = {
    0,    1,    2,    3,    4,    5,     6,     7,     8,     9,    10,   15,
    20,   25,   30,   40,   50,   60,    70,    80,    90,    100,  150,  200,
    250,  300,  400,  500,  600,  700,   800,   900,   1000,  2000, 3000, 4000,
    5000, 6000, 7000, 8000, 9000, 10000, 15000, 20000, 30000, 60000};

auto &frame_id_deltas_family =
    prometheus::BuildHistogram().Name("frame_id_delta").Register(metrics_registry());

auto &frame_time_delta_millis_family = prometheus::BuildHistogram()
                                           .Name("frame_time_delta_millis")
                                           .Register(metrics_registry());

auto &frame_departure_time_delta_millis_family =
    prometheus::BuildHistogram()
        .Name("frame_departure_time_delta_millis")
        .Register(metrics_registry());

auto &frame_arrival_time_delta_millis_family =
    prometheus::BuildHistogram()
        .Name("frame_arrival_time_delta_millis")
        .Register(metrics_registry());

auto &frame_departure_time_jitter_family = prometheus::BuildHistogram()
                                               .Name("frame_departure_time_jitter")
                                               .Register(metrics_registry());

auto &frame_arrival_time_jitter_family = prometheus::BuildHistogram()
                                             .Name("frame_arrival_time_jitter")
                                             .Register(metrics_registry());

auto &frame_delivery_delay_millis_family = prometheus::BuildHistogram()
                                               .Name("frame_delivery_delay_millis")
                                               .Register(metrics_registry());

double observe_time_delta(const std::chrono::system_clock::time_point &t1,
                          const std::chrono::system_clock::time_point &t2,
                          prometheus::Histogram &histogram) {
  const double delta =
      std::abs(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t2).count());
  histogram.Observe(delta);
  return delta;
}

struct network_metrics_collector : boost::static_visitor<void> {
 public:
  explicit network_metrics_collector(const std::string &channel)
      : _frame_id_deltas(frame_id_deltas_family.Add(
            {{"channel", channel}}, std::vector<double>(id_delta_buckets))),
        _frame_time_delta_millis(frame_time_delta_millis_family.Add(
            {{"channel", channel}}, std::vector<double>(time_delta_buckets))),
        _frame_departure_time_delta_millis(frame_departure_time_delta_millis_family.Add(
            {{"channel", channel}}, std::vector<double>(time_delta_buckets))),
        _frame_arrival_time_delta_millis(frame_arrival_time_delta_millis_family.Add(
            {{"channel", channel}}, std::vector<double>(time_delta_buckets))),
        _frame_delivery_delay_millis(frame_delivery_delay_millis_family.Add(
            {{"channel", channel}}, std::vector<double>(delivery_delay_buckets))),
        _frame_arrival_time_jitter(frame_arrival_time_jitter_family.Add(
            {{"channel", channel}}, std::vector<double>(jitter_buckets))),
        _frame_departure_time_jitter(frame_departure_time_jitter_family.Add(
            {{"channel", channel}}, std::vector<double>(jitter_buckets))) {}

  void operator()(const network_metadata & /*m*/) {}

  void operator()(const network_frame &f) {
    if (_first_frame) {
      _first_frame = false;
    } else {
      _frame_id_deltas.Observe(std::abs(f.id.i1 - _last_id.i1));

      observe_time_delta(f.t, _last_time, _frame_time_delta_millis);
      const double departure_time_delta = observe_time_delta(
          f.dt, _last_departure_time, _frame_departure_time_delta_millis);
      const double arrival_time_delta = observe_time_delta(
          f.arrival_time, _last_arrival_time, _frame_arrival_time_delta_millis);

      _frame_delivery_delay_millis.Observe(
          std::abs(departure_time_delta - arrival_time_delta));

      _departure_time_jitter.emplace(departure_time_delta);
      _arrival_time_jitter.emplace(arrival_time_delta);

      _frame_departure_time_jitter.Observe(_departure_time_jitter.value());
      _frame_arrival_time_jitter.Observe(_arrival_time_jitter.value());
    }

    _last_id = f.id;
    _last_time = f.t;
    _last_departure_time = f.dt;
    _last_arrival_time = f.arrival_time;
  }

 private:
  prometheus::Histogram &_frame_id_deltas;
  prometheus::Histogram &_frame_time_delta_millis;
  prometheus::Histogram &_frame_arrival_time_delta_millis;
  prometheus::Histogram &_frame_departure_time_delta_millis;
  prometheus::Histogram &_frame_delivery_delay_millis;
  prometheus::Histogram &_frame_departure_time_jitter;
  prometheus::Histogram &_frame_arrival_time_jitter;

  bool _first_frame{true};
  frame_id _last_id;
  std::chrono::system_clock::time_point _last_time;
  std::chrono::system_clock::time_point _last_departure_time;
  std::chrono::system_clock::time_point _last_arrival_time;

  // TODO: prometheus can probably calculate standard deviation on it's own.
  statsutils::std_dev _departure_time_jitter{1000};
  statsutils::std_dev _arrival_time_jitter{1000};
};
}  // namespace

streams::op<network_packet, network_packet> report_video_metrics(
    const std::string &channel_name) {
  return [channel_name](streams::publisher<network_packet> &&src) {
    network_metrics_collector visitor(channel_name);
    return std::move(src)
           >> streams::map([visitor = std::move(visitor)](network_packet && p) mutable {
               boost::apply_visitor(visitor, p);
               return std::move(p);
             });
  };
}

}  // namespace video
}  // namespace satori
