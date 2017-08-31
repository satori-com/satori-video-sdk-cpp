#define BOOST_TEST_MODULE StreamsTest
#include <boost/test/included/unit_test.hpp>

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "streams.h"

namespace {
template <typename T>
std::vector<std::string> events(streams::publisher<T> &&p) {
  struct subscriber : streams::subscriber<T> {
    std::vector<std::string> events;

    void on_next(T &&t) override { events.push_back(std::to_string(t)); }
    void on_error(const std::string &message) override { BOOST_ASSERT(false); }
    void on_complete() override { events.push_back("."); }
    void on_subscribe(streams::subscription &s) override {
      s.request(std::numeric_limits<int>::max());
    }
  };
  auto s = new subscriber;
  p->subscribe(*s);
  return s->events;
}

std::vector<std::string> strings(std::initializer_list<std::string> strs) {
  return std::vector<std::string>(strs);
}
}  // namespace

BOOST_TEST_SPECIALIZED_COLLECTION_COMPARE(std::vector<std::string>)

BOOST_AUTO_TEST_CASE(empty) {
  auto p = streams::publishers<int>::empty();
  BOOST_TEST(events(std::move(p)) == strings({"."}));
}

BOOST_AUTO_TEST_CASE(of) {
  auto p = streams::publishers<int>::of({3, 1, 2});
  BOOST_TEST(events(std::move(p)) == strings({"3", "1", "2", "."}));
}

BOOST_AUTO_TEST_CASE(range) {
  auto p = streams::publishers<int>::range(0, 3);
  BOOST_TEST(events(std::move(p)) == strings({"0", "1", "2", "."}));
}

BOOST_AUTO_TEST_CASE(map) {
  auto p = streams::publishers<int>::range(2, 5) |
           streams::map([](int i) { return i * i; });
  BOOST_TEST(events(std::move(p)) == strings({"4", "9", "16", "."}));
}

BOOST_AUTO_TEST_CASE(test_flat_map) {
  auto idx = streams::publishers<int>::range(1, 4);
  auto p = std::move(idx) | streams::flat_map([](int i) {
             return streams::publishers<int>::range(0, i);
           });
  auto e = events(std::move(p));
  BOOST_TEST(e == strings({"0", "0", "1", "0", "1", "2", "."}));
}