#include "tele_impl.h"

#include <unistd.h>
#include <boost/assert.hpp>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

namespace tele {

namespace {
boost::posix_time::seconds tele_interval(1);
bool tele_running{false};

tele::counter *messages_published = tele::counter_new("tele", "messages_published");

static std::string get_node_id() {
  char *node_id = getenv("NODE_ID");
  if (node_id != nullptr) return std::string(node_id);

  char hostname[255] = {0};
  int err = gethostname(hostname, sizeof(hostname));
  BOOST_VERIFY(!err);
  return std::string(hostname, strnlen(hostname, sizeof(hostname)));
}

template <typename T>
class cell {
 public:
  cell(const char *group, const char *name)
      : _name(std::string(group) + "." + std::string(name)) {}

  std::string full_name() const { return _name; }

  const T &value() const { return _value; }

 protected:
  std::string _name;
  T _value{0};
};

cbor_item_t *int64_to_cbor(int64_t v) {
  return v > 0 ? cbor_build_uint64(v) : cbor_build_negint64(-v);
}

}  // namespace

struct counter : public cell<std::atomic_uint_fast64_t> {
  counter(const char *group, const char *name)
      : cell<std::atomic_uint_fast64_t>(group, name) {}

  static std::vector<counter *> &counters() {
    static std::vector<counter *> static_counters;
    return static_counters;
  }

  void inc(uint64_t delta) {
    BOOST_VERIFY(delta >= 0);
    _value += delta;
  }

  cbor_item_t *to_cbor() { return cbor_build_uint64(_value.load()); }
};

EXPORT counter *counter_new(const char *group, const char *name) noexcept {
  auto c = new counter(group, name);
  counter::counters().push_back(c);
  return c;
}

EXPORT void counter_delete(counter *c) noexcept {
  auto &ctrs = counter::counters();
  auto it = std::find(ctrs.begin(), ctrs.end(), c);
  BOOST_ASSERT(it != ctrs.end());
  ctrs.erase(it);
  delete c;
}

EXPORT void counter_inc(counter *counter, uint64_t delta) noexcept {
  counter->inc(delta);
}

struct gauge : public cell<std::atomic_int_fast64_t> {
  gauge(const char *group, const char *name)
      : cell<std::atomic_int_fast64_t>(group, name) {}

  static std::vector<gauge *> &gauges() {
    static std::vector<gauge *> static_gauges;
    return static_gauges;
  }

  void set(int64_t value) { _value = value; }

  cbor_item_t *to_cbor() {
    int64_t v = _value.load();
    return v > 0 ? cbor_build_uint64(v) : cbor_build_negint64(-v);
  }
};

EXPORT gauge *gauge_new(const char *group, const char *name) noexcept {
  auto g = new gauge(group, name);
  gauge::gauges().push_back(g);
  return g;
}

EXPORT void gauge_set(gauge *gauge, int64_t value) noexcept { gauge->set(value); }

struct distribution : public cell<std::vector<int64_t>> {
  static constexpr size_t max_distribution_size = 100;

  distribution(const char *group, const char *name)
      : cell<std::vector<int64_t>>(group, name) {}

  static std::vector<distribution *> &distributions() {
    static std::vector<distribution *> static_distributions;
    return static_distributions;
  }

  void add(int64_t value) {
    if (!tele_running) return;

    // todo: calculate distribution statistics rather than send it to the server
    std::lock_guard<std::mutex> guard(_mutex);
    if (_value.size() > max_distribution_size) {
      std::cerr << "distribution too large: " << _value.size() << "\n";
    }
    _value.push_back(value);
  }

  cbor_item_t *to_cbor() {
    std::lock_guard<std::mutex> guard(_mutex);
    cbor_item_t *result = cbor_new_definite_array(_value.size());
    for (size_t i = 0; i < _value.size(); ++i) {
      cbor_array_set(result, i, int64_to_cbor(_value[i]));
    }
    return result;
  }

  void clear() {
    std::lock_guard<std::mutex> guard(_mutex);
    _value.clear();
  }

 private:
  std::mutex _mutex;
};

EXPORT distribution *distribution_new(const char *group, const char *name) noexcept {
  auto d = new distribution(group, name);
  distribution::distributions().push_back(d);
  return d;
}

EXPORT void distribution_add(distribution *distribution, int64_t value) noexcept {
  distribution->add(value);
}

template <typename Cell>
cbor_item_t *tele_sereialize_cells(std::vector<Cell *> &cells) {
  cbor_item_t *cells_map = cbor_new_definite_map(cells.size());
  for (int i = 0; i < cells.size(); ++i) {
    Cell *cell = cells[i];
    cbor_map_add(cells_map,
                 {.key = cbor_move(cbor_build_string(cell->full_name().c_str())),
                  .value = cbor_move(cell->to_cbor())});
  }
  return cells_map;
}

cbor_item_t *tele_serialize(std::vector<counter *> &counters,
                            std::vector<gauge *> &gauges,
                            std::vector<distribution *> &distributions) {
  cbor_item_t *root = cbor_new_indefinite_map();
  cbor_map_add(root,
               {.key = cbor_move(cbor_build_string("id")),
                .value = cbor_move(cbor_build_string(get_node_id().c_str()))});

  cbor_map_add(root,
               {.key = cbor_move(cbor_build_string("counters")),
                .value = cbor_move(tele_sereialize_cells(counters))});

  cbor_map_add(root,
               {.key = cbor_move(cbor_build_string("gauges")),
                .value = cbor_move(tele_sereialize_cells(gauges))});

  cbor_map_add(root,
               {.key = cbor_move(cbor_build_string("distributions")),
                .value = cbor_move(tele_sereialize_cells(distributions))});
  for (auto distribution : distributions) {
    distribution->clear();
  }

  return root;
}

void on_tele_tick(rtm::publisher &publisher, boost::asio::deadline_timer &timer,
                  const boost::system::error_code &ec) {
  if (ec == boost::asio::error::operation_aborted) return;
  BOOST_ASSERT(!ec);
  counter_inc(messages_published);
  publisher.publish(tele::channel, tele_serialize(counter::counters(), gauge::gauges(),
                                                  distribution::distributions()));
  timer.expires_at(timer.expires_at() + tele_interval);
  timer.async_wait([&publisher, &timer](const boost::system::error_code &ec) {
    on_tele_tick(publisher, timer, ec);
  });
}

publisher::publisher(rtm::publisher &rtm_publisher, boost::asio::io_service &io_service)
    : _timer(io_service, tele_interval) {
  tele_running = true;
  on_tele_tick(rtm_publisher, _timer, boost::system::error_code{});
}

publisher::~publisher() { _timer.cancel(); }

}  // namespace tele
