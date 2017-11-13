#define BOOST_TEST_MODULE StreamsTest
#define BOOST_TEST_NO_MAIN
#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/included/unit_test.hpp>

#include <chrono>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "logging_impl.h"
#include "streams/asio_streams.h"
#include "streams/streams.h"
#include "streams/threaded_worker.h"

using namespace satori::video;

namespace {

template <typename T>
size_t run_wait(boost::asio::io_service &io, const streams::deferred<T> &d) {
  size_t result = 0;
  while (!d.resolved()) {
    result += io.run();
  }
  return result;
}

template <typename T>
void spin_wait(const streams::deferred<T> &d, std::chrono::milliseconds delay) {
  while (!d.resolved()) {
    std::this_thread::sleep_for(delay);
  }
}

template <typename T>
std::vector<std::string> events(streams::publisher<T> &&p,
                                boost::asio::io_service *io = nullptr) {
  std::vector<std::string> events;

  auto when_done =
      p->process([&events](T &&t) mutable { events.push_back(std::to_string(t)); });
  when_done.on([&events](std::error_condition ec) {
    if (ec) {
      events.push_back("error:" + ec.message());
    } else {
      events.emplace_back(".");
    }
  });

  if (io) {
    run_wait(*io, when_done);
  } else {
    spin_wait(when_done, std::chrono::milliseconds(1));
  }

  BOOST_TEST(when_done.resolved());
  return events;
}

std::vector<std::string> strings(std::initializer_list<std::string> strs) {
  return std::vector<std::string>(strs);
}
}  // namespace

BOOST_TEST_SPECIALIZED_COLLECTION_COMPARE(std::vector<std::string>)

BOOST_AUTO_TEST_CASE(empty) {
  auto p = streams::publishers::empty<int>();
  BOOST_TEST(events(std::move(p)) == strings({"."}));
}

BOOST_AUTO_TEST_CASE(of_initializer_list) {
  auto p = streams::publishers::of({3, 1, 2});
  BOOST_TEST(events(std::move(p)) == strings({"3", "1", "2", "."}));
}

BOOST_AUTO_TEST_CASE(of_vector) {
  auto p = streams::publishers::of(std::vector<int>{3, 1, 2});
  BOOST_TEST(events(std::move(p)) == strings({"3", "1", "2", "."}));
}

BOOST_AUTO_TEST_CASE(of_queue) {
  std::queue<int> q;
  q.push(3);
  q.push(1);
  q.push(2);
  auto p = streams::publishers::of(std::move(q));
  BOOST_TEST(events(std::move(p)) == strings({"3", "1", "2", "."}));
}

BOOST_AUTO_TEST_CASE(of_list) {
  auto p = streams::publishers::of(std::list<int>{3, 1, 2});
  BOOST_TEST(events(std::move(p)) == strings({"3", "1", "2", "."}));
}

BOOST_AUTO_TEST_CASE(range) {
  auto p = streams::publishers::range(0, 3);
  BOOST_TEST(events(std::move(p)) == strings({"0", "1", "2", "."}));
}

BOOST_AUTO_TEST_CASE(map) {
  auto p = streams::publishers::range(2, 5) >> streams::map([](int i) { return i * i; });
  BOOST_TEST(events(std::move(p)) == strings({"4", "9", "16", "."}));
}

BOOST_AUTO_TEST_CASE(flat_map) {
  LOG_SCOPE_FUNCTION(ERROR);
  auto idx = streams::publishers::range(1, 4);
  auto p = std::move(idx)
           >> streams::flat_map([](int i) { return streams::publishers::range(0, i); });
  auto e = events(std::move(p));
  BOOST_TEST(e == strings({"0", "0", "1", "0", "1", "2", "."}));
}

BOOST_AUTO_TEST_CASE(head) {
  auto p = streams::publishers::range(3, 300000000) >> streams::head();
  BOOST_TEST(events(std::move(p)) == strings({"3", "."}));
}

BOOST_AUTO_TEST_CASE(take) {
  auto p = streams::publishers::range(2, 300000000) >> streams::take(4);
  BOOST_TEST(events(std::move(p)) == strings({"2", "3", "4", "5", "."}));
}

BOOST_AUTO_TEST_CASE(take_while) {
  auto p = streams::publishers::range(2, 300000000)
           >> streams::take_while([](const int &i) { return i < 10; });
  BOOST_TEST(events(std::move(p))
             == strings({"2", "3", "4", "5", "6", "7", "8", "9", "."}));
}

BOOST_AUTO_TEST_CASE(concat) {
  auto p1 = streams::publishers::range(1, 3);
  auto p2 = streams::publishers::range(3, 6);
  auto p = streams::publishers::concat(std::move(p1), std::move(p2));
  BOOST_TEST(events(std::move(p)) == strings({"1", "2", "3", "4", "5", "."}));
}

BOOST_AUTO_TEST_CASE(on_finally_empty) {
  LOG_SCOPE_FUNCTION(ERROR);
  bool terminated = false;
  auto p = streams::publishers::empty<int>()
           >> streams::do_finally([&terminated]() { terminated = true; });
  BOOST_TEST(!terminated);
  events(std::move(p));
  BOOST_TEST(terminated);
}

BOOST_AUTO_TEST_CASE(on_finally_error) {
  bool terminated = false;
  auto p = streams::publishers::error<int>(std::errc::not_supported)
           >> streams::do_finally([&terminated]() { terminated = true; });
  BOOST_TEST(!terminated);
  events(std::move(p));
  BOOST_TEST(terminated);
}

BOOST_AUTO_TEST_CASE(on_finally_unsubscribe) {
  bool terminated = false;
  auto p = streams::publishers::range(3, 300000000)
           >> streams::do_finally([&terminated]() { terminated = true; })
           >> streams::head();
  BOOST_TEST(!terminated);
  events(std::move(p));
  BOOST_TEST(terminated);
}

streams::op<int, int> square() {
  return [](streams::publisher<int> &&src) {
    return std::move(src) >> streams::map([](int i) { return i * i; });
  };
}

BOOST_AUTO_TEST_CASE(lift_square) {
  auto p = streams::publishers::range(2, 5) >> square();
  BOOST_TEST(events(std::move(p)) == strings({"4", "9", "16", "."}));
}

BOOST_AUTO_TEST_CASE(threaded_worker) {
  LOG_SCOPE_FUNCTION(0);
  auto p = streams::publishers::range(1, 5) >> streams::threaded_worker("test")
           >> streams::flatten();
  BOOST_TEST(events(std::move(p)) == strings({"1", "2", "3", "4", "."}));
}

BOOST_AUTO_TEST_CASE(threaded_worker_cancel) {
  LOG_SCOPE_FUNCTION(0);
  auto p = streams::publishers::range(1, 5) >> streams::threaded_worker("test")
           >> streams::flatten() >> streams::take(3);
  BOOST_TEST(events(std::move(p)) == strings({"1", "2", "3", "."}));
}

BOOST_AUTO_TEST_CASE(async_cancel) {
  LOG_SCOPE_FUNCTION(0);
  struct async_source {
    void start(streams::observer<int> *s) {
      std::thread{[this, s]() {
        int counter = 1;
        while (_active) {
          int i = counter++;
          LOG(1) << "on_next " << i;
          s->on_next(std::move(i));
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        delete this;
      }}
          .detach();
    }

    void stop() { _active = false; }

    std::atomic<bool> _active{true};
  };

  auto p = streams::generators<int>::async<async_source>(
               [](streams::observer<int> &o) {
                 auto src = new async_source();
                 src->start(&o);
                 return src;
               },
               [](async_source *src) { src->stop(); })
           >> streams::flatten() >> streams::take(3);

  BOOST_TEST(events(std::move(p)) == strings({"1", "2", "3", "."}));
}

BOOST_AUTO_TEST_CASE(delay_finally) {
  boost::asio::io_service io_service;
  bool terminated = false;
  auto p = streams::publishers::of({1, 2, 3, 4, 5})
           >> streams::asio::interval<int>(io_service, std::chrono::milliseconds(5))
           >> streams::do_finally([&terminated]() { terminated = true; });
  BOOST_TEST(!terminated);
  BOOST_TEST(events(std::move(p), &io_service)
             == strings({"1", "2", "3", "4", "5", "."}));
  BOOST_TEST(terminated);
}

template <typename T>
struct collector_sink : public streams::subscriber<T> {
  void on_next(T &&t) override {
    BOOST_TEST(src);
    items.push_back(t);
    src->request(1);
  }

  void on_error(std::error_condition /*ec*/) override { error = true; }

  void on_complete() override { complete = true; }

  void on_subscribe(streams::subscription &s) override {
    BOOST_TEST(!src);
    src = &s;
    src->request(1);
  }

  streams::subscription *src;
  std::vector<T> items;
  bool error{false};
  bool complete{false};
};

BOOST_AUTO_TEST_CASE(collector_asio) {
  boost::asio::io_service io_service;
  bool terminated = false;
  auto p = streams::publishers::range(1, 300000000)
           >> streams::asio::interval<int>(io_service, std::chrono::milliseconds(5))
           >> streams::take_while([](auto i) { return i < 10; })
           >> streams::do_finally([&terminated]() { terminated = true; });
  auto s = std::make_unique<collector_sink<int>>();
  BOOST_TEST(!terminated);

  p->subscribe(*s);
  io_service.run();
  std::vector<int> expected{1, 2, 3, 4, 5, 6, 7, 8, 9};
  BOOST_TEST(expected == s->items);
  BOOST_TEST(s->complete);
  BOOST_TEST(!s->error);

  BOOST_TEST(terminated);
}

BOOST_AUTO_TEST_CASE(merge) {
  auto p1 = streams::publishers::range(1, 3);
  auto p2 = streams::publishers::range(3, 6);
  auto p = streams::publishers::merge(std::move(p1), std::move(p2));
  auto e = events(std::move(p));
  std::sort(e.begin(), e.end());
  BOOST_TEST(e == strings({".", "1", "2", "3", "4", "5"}));
}

BOOST_AUTO_TEST_CASE(merge_with_take_less) {
  auto p = streams::publishers::merge(streams::publishers::range(1, 300000000),
                                      streams::publishers::range(1, 300000000));
  auto e = events(std::move(p) >> streams::take(3));
  BOOST_TEST(3 + 1 == e.size());
}

BOOST_AUTO_TEST_CASE(merge_with_take_more) {
  auto p = streams::publishers::merge(streams::publishers::range(1, 3),
                                      streams::publishers::range(1, 3));
  auto e = events(std::move(p) >> streams::take(10));
  std::sort(e.begin(), e.end());
  BOOST_TEST(e == strings({".", "1", "1", "2", "2"}));
}

BOOST_AUTO_TEST_CASE(merge_with_one_empty) {
  auto p = streams::publishers::merge(streams::publishers::of(std::vector<int>{}),
                                      streams::publishers::range(1, 3));
  auto e = events(std::move(p));
  std::sort(e.begin(), e.end());
  BOOST_TEST(e == strings({".", "1", "2"}));
}

BOOST_AUTO_TEST_CASE(merge_with_both_empty) {
  auto p = streams::publishers::merge(streams::publishers::of(std::vector<int>{}),
                                      streams::publishers::of(std::vector<int>{}));
  auto e = events(std::move(p));
  std::sort(e.begin(), e.end());
  BOOST_TEST(e == strings({"."}));
}

int main(int argc, char *argv[]) {
  init_logging(argc, argv);
  return boost::unit_test::unit_test_main(init_unit_test, argc, argv);
}