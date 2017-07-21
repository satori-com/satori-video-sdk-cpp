// RTM client interface definition.
#pragma once

#include <cbor.h>
#include <rapidjson/document.h>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/assert.hpp>
#include <boost/optional.hpp>
#include <memory>
#include <string>
#include <vector>

namespace rtm {

enum class error : unsigned char {
  Unknown = 1,
  NotConnected = 2,
  ResponseParsingError = 3,
  InvalidResponse = 4,
  SubscriptionError = 5
};

struct error_callbacks {
  virtual ~error_callbacks() = default;

  virtual void on_error(error /*error*/, const std::string & /*message*/) {}
};

struct channel_position {
  uint32_t gen;
  uint64_t pos;

  std::string str() const {
    return std::to_string(gen) + ":" + std::to_string(pos);
  }

  static channel_position parse(const std::string &str) {
    char *str_pos = nullptr;
    auto gen = strtoll(str.c_str(), &str_pos, 10);
    BOOST_ASSERT(gen <= std::numeric_limits<uint32_t>::max());
    if ((str_pos == nullptr) || str_pos == str.c_str() || *str_pos != ':') {
      return {0, 0};
    }
    auto pos = strtoull(str_pos + 1, &str_pos, 10);
    BOOST_ASSERT(pos <= std::numeric_limits<uint64_t>::max());
    if ((str_pos == nullptr) || (*str_pos != 0)) {
      return {0, 0};
    }
    return {static_cast<uint32_t>(gen), static_cast<uint64_t>(pos)};
  }
};

struct publish_callbacks : public error_callbacks {
  ~publish_callbacks() override = default;

  virtual void on_ok(const channel_position & /*position*/) {}
};

struct publisher {
  virtual ~publisher() = default;

  virtual void publish(const std::string &channel, const cbor_item_t *message,
                       publish_callbacks *callbacks = nullptr) = 0;
};

// Subscription interface of RTM.
struct subscription {};

struct subscription_callbacks : public error_callbacks {
  virtual void on_data(const subscription & /*subscription*/,
                       const rapidjson::Value & /*unused*/) {}
};

struct history_options {
  boost::optional<uint64_t> count;
  boost::optional<uint64_t> age;
};

struct subscription_options {
  bool force{false};
  bool fast_forward{true};
  boost::optional<channel_position> position;
  history_options history;
};

struct subscriber {
  virtual ~subscriber() = default;

  virtual void subscribe_channel(
      const std::string &channel, subscription &sub,
      subscription_callbacks &callbacks,
      const subscription_options *options = nullptr) = 0;

  virtual void subscribe_filter(
      const std::string &filter, subscription &sub,
      subscription_callbacks &callbacks,
      const subscription_options *options = nullptr) = 0;

  virtual void unsubscribe(subscription &sub) = 0;

  virtual const channel_position &position(subscription &sub) = 0;

  virtual bool is_up(subscription &sub) = 0;
};

class client : public publisher, public subscriber {};

std::unique_ptr<client> new_client(const std::string &endpoint,
                                   const std::string &port,
                                   const std::string &appkey,
                                   boost::asio::io_service &io_service,
                                   boost::asio::ssl::context &ssl_ctx,
                                   size_t id, error_callbacks &callbacks);
}
