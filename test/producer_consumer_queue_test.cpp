#define BOOST_TEST_MODULE ProducerConsumerQueueTest
#include <boost/test/included/unit_test.hpp>

#include <thread>
#include "producer_consumer_queue.h"

BOOST_AUTO_TEST_CASE(simple) {
  producer_consumer_queue<int> queue;
  int polled_value;

  {
    std::thread t([&queue]() { queue.put(1); });
    t.join();
  }

  BOOST_TEST(queue.poll(polled_value));
  BOOST_CHECK_EQUAL(1, polled_value);

  BOOST_TEST(!queue.poll(polled_value));

  {
    std::thread t([&queue]() { queue.put(2); });
    t.join();
  }
  {
    std::thread t([&queue]() { queue.put(3); });
    t.join();
  }

  BOOST_TEST(queue.poll(polled_value));
  BOOST_CHECK_EQUAL(2, polled_value);

  BOOST_TEST(queue.poll(polled_value));
  BOOST_CHECK_EQUAL(3, polled_value);

  BOOST_TEST(!queue.poll(polled_value));
}