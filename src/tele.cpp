#include "tele_impl.h"

#include <unistd.h>
#include <boost/assert.hpp>
#include <string>
#include <vector>

namespace tele {

namespace {
boost::posix_time::seconds tele_interval(1);

tele::counter *messages_published =
    tele::counter_new("tele", "messages_published");

static std::string get_node_id() {
  char *node_id = getenv("NODE_ID");
  if (node_id != nullptr) return std::string(node_id);

  char hostname[255] = {0};
  int err = gethostname(hostname, sizeof(hostname));
  BOOST_VERIFY(!err);
  return std::string(hostname, strnlen(hostname, sizeof(hostname)));
}
}  // namespace

struct counter {
 public:
  counter(const char *group, const char *name)
      : _name(std::string(group) + "." + std::string(name)) {}
  static std::vector<counter *> &counters() {
    static std::vector<counter *> static_counters;
    return static_counters;
  }

  void inc(uint64_t delta) {
    BOOST_VERIFY(delta >= 0);
    _value += delta;
  }

  std::string full_name() const { return _name; }

  uint64_t value() const { return _value; }

 private:
  std::string _name;
  uint64_t _value{0};
};

EXPORT counter *counter_new(const char *group, const char *name) {
  auto c = new counter(group, name);
  counter::counters().push_back(c);
  return c;
}

EXPORT void counter_inc(counter *counter, uint64_t delta) {
  counter->inc(delta);
}

cbor_item_t *tele_serialize(std::vector<counter *> &counters) {
  cbor_item_t *root = cbor_new_definite_map(2);
  cbor_map_add(root,
               {.key = cbor_move(cbor_build_string("id")),
                .value = cbor_move(cbor_build_string(get_node_id().c_str()))});

  cbor_item_t *counters_map = cbor_new_definite_map(counters.size());
  for (int i = 0; i < counters.size(); ++i) {
    counter *counter = counters[i];
    cbor_map_add(
        counters_map,
        {.key = cbor_move(cbor_build_string(counter->full_name().c_str())),
         .value = cbor_move(cbor_build_negint64(counter->value()))});
  }

  cbor_map_add(root, {.key = cbor_move(cbor_build_string("counters")),
                      .value = cbor_move(counters_map)});

  return root;
}

void on_tele_tick(rtm::publisher &publisher, boost::asio::deadline_timer &timer,
                  const boost::system::error_code & /*e*/) {
  counter_inc(messages_published);
  publisher.publish(tele::channel, tele_serialize(counter::counters()));
  timer.async_wait([&publisher, &timer](const boost::system::error_code &ec) {
    timer.expires_at(timer.expires_at() + tele_interval);
    on_tele_tick(publisher, timer, ec);
  });
}

publisher::publisher(rtm::publisher &rtm_publisher,
                     boost::asio::io_service &io_service)
    : _timer(io_service, tele_interval) {
  on_tele_tick(rtm_publisher, _timer, boost::system::error_code{});
}
}  // namespace tele
