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
  std::vector<std::string> events;
  p->process([&events](T &&t) mutable { events.push_back(std::to_string(t)); },
             [&events]() mutable { events.push_back("."); },
             [&events](const std::string &message) mutable {
               events.push_back("error:" + message);
             });
  return events;
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
  auto p =
      streams::publishers<int>::range(2, 5) >> streams::map([](int i) { return i * i; });
  BOOST_TEST(events(std::move(p)) == strings({"4", "9", "16", "."}));
}

BOOST_AUTO_TEST_CASE(flat_map) {
  auto idx = streams::publishers<int>::range(1, 4);
  auto p = std::move(idx) >>
           streams::flat_map([](int i) { return streams::publishers<int>::range(0, i); });
  auto e = events(std::move(p));
  BOOST_TEST(e == strings({"0", "0", "1", "0", "1", "2", "."}));
}

BOOST_AUTO_TEST_CASE(head) {
  auto p = streams::publishers<int>::range(3, 300000000) >> streams::head();
  BOOST_TEST(events(std::move(p)) == strings({"3", "."}));
}

BOOST_AUTO_TEST_CASE(take) {
  auto p = streams::publishers<int>::range(2, 300000000) >> streams::take(4);
  BOOST_TEST(events(std::move(p)) == strings({"2", "3", "4", "5", "."}));
}

BOOST_AUTO_TEST_CASE(merge) {
  auto p1 = streams::publishers<int>::range(1, 3);
  auto p2 = streams::publishers<int>::range(3, 6);
  std::vector<streams::publisher<int>> pp;
  pp.push_back(std::move(p1));
  pp.push_back(std::move(p2));
  auto p = streams::publishers<int>::merge(std::move(pp));
  BOOST_TEST(events(std::move(p)) == strings({"1", "2", "3", "4", "5", "."}));
}

BOOST_AUTO_TEST_CASE(on_finally_empty) {
  bool terminated = false;
  auto p = streams::publishers<int>::empty() >>
           streams::do_finally([&terminated]() { terminated = true; });
  BOOST_TEST(!terminated);
  events(std::move(p));
  BOOST_TEST(terminated);
}

BOOST_AUTO_TEST_CASE(on_finally_error) {
  bool terminated = false;
  auto p = streams::publishers<int>::error("") >>
           streams::do_finally([&terminated]() { terminated = true; });
  BOOST_TEST(!terminated);
  events(std::move(p));
  BOOST_TEST(terminated);
}

BOOST_AUTO_TEST_CASE(on_finally_unsubscribe) {
  bool terminated = false;
  auto p = streams::publishers<int>::range(3, 300000000) >>
           streams::do_finally([&terminated]() { terminated = true; }) >> streams::head();
  BOOST_TEST(!terminated);
  events(std::move(p));
  BOOST_TEST(terminated);
}

streams::op<int, int> square() {
  return [](streams::publisher<int> &&src) {
    return std::move(src) >> streams::map([](int i) { return i * i; });
  };
};

BOOST_AUTO_TEST_CASE(lift_square) {
  auto p = streams::publishers<int>::range(2, 5) >> streams::lift(square());
  BOOST_TEST(events(std::move(p)) == strings({"4", "9", "16", "."}));
}