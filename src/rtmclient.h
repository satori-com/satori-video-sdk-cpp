// RTM client interface definition.
#pragma once

#include <cbor.h>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/optional.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>

#include "logging.h"

namespace rtm {

enum class client_error : unsigned char {
  // 0 - not used, success.

  Unknown = 1,
  NotConnected = 2,
  ResponseParsingError = 3,
  InvalidResponse = 4,
  SubscriptionError = 5,
  SubscribeError = 6,
  UnsubscribeError = 7
};

std::error_condition make_error_condition(client_error e);

struct error_callbacks {
  virtual ~error_callbacks() = default;

  virtual void on_error(std::error_condition ec) {}
};

struct channel_position {
  uint32_t gen{0};
  uint64_t pos{0};

  std::string str() const { return std::to_string(gen) + ":" + std::to_string(pos); }

  static channel_position parse(const std::string &str) {
    char *str_pos = nullptr;
    auto gen = strtoll(str.c_str(), &str_pos, 10);
    CHECK_LE(gen, std::numeric_limits<uint32_t>::max());
    if ((str_pos == nullptr) || str_pos == str.c_str() || *str_pos != ':') {
      return {0, 0};
    }
    auto pos = strtoull(str_pos + 1, &str_pos, 10);
    CHECK_LE(pos, std::numeric_limits<uint64_t>::max());
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
  virtual void on_data(const subscription & /*subscription*/, cbor_item_t * /*unused*/) {}
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

  virtual void subscribe_channel(const std::string &channel, const subscription &sub,
                                 subscription_callbacks &callbacks,
                                 const subscription_options *options = nullptr) = 0;

  virtual void subscribe_filter(const std::string &filter, const subscription &sub,
                                subscription_callbacks &callbacks,
                                const subscription_options *options = nullptr) = 0;

  virtual void unsubscribe(const subscription &sub) = 0;

  virtual channel_position position(const subscription &sub) = 0;

  virtual bool is_up(const subscription &sub) = 0;
};

class client : public publisher, public subscriber {
 public:
  virtual void start() = 0;
  virtual void stop() = 0;
};

std::unique_ptr<client> new_client(const std::string &endpoint, const std::string &port,
                                   const std::string &appkey,
                                   boost::asio::io_service &io_service,
                                   boost::asio::ssl::context &ssl_ctx, size_t id,
                                   error_callbacks &callbacks);

// reconnects on errors.
// todo(mike): once I have several error reports, I will figure out how to handle those.
class resilient_client : public client {
 public:
  using client_factory_t = std::function<std::unique_ptr<client>()>;

  explicit resilient_client(client_factory_t &&factory) : _factory(std::move(factory)) {
    _client = _factory();
  }

  void publish(const std::string &channel, const cbor_item_t *message,
               publish_callbacks *callbacks) override {
    std::lock_guard<std::mutex> guard(_client_mutex);
    _client->publish(channel, message, callbacks);
  }

  void subscribe_channel(const std::string &channel, const subscription &sub,
                         subscription_callbacks &callbacks,
                         const subscription_options *options) override {
    std::lock_guard<std::mutex> guard(_client_mutex);
    _subscriptions.push_back({channel, &sub, &callbacks, options});
    _client->subscribe_channel(channel, sub, callbacks, options);
  }

  void subscribe_filter(const std::string &filter, const subscription &sub,
                        subscription_callbacks &callbacks,
                        const subscription_options *options) override {
    ABORT() << "not implemented";
  }

  void unsubscribe(const subscription &sub) override {
    std::lock_guard<std::mutex> guard(_client_mutex);
    _client->unsubscribe(sub);
    std::remove_if(_subscriptions.begin(), _subscriptions.end(),
                   [&sub](const subscription_info &si) { return &sub == si.sub; });
  }

  channel_position position(const subscription &sub) override {
    std::lock_guard<std::mutex> guard(_client_mutex);
    return _client->position(sub);
  }

  bool is_up(const subscription &sub) override { return _client->is_up(sub); }

  void start() override { _client->start(); }

  void stop() override { _client->stop(); }

 private:
  struct subscription_info {
    std::string channel;
    const subscription *sub;
    subscription_callbacks *callbacks;
    const subscription_options *options;
  };

  client_factory_t _factory;
  std::unique_ptr<client> _client;
  std::mutex _client_mutex;

  std::vector<subscription_info> _subscriptions;
};

}  // namespace rtm

namespace std {
template <>
struct is_error_condition_enum<rtm::client_error> : std::true_type {};
}  // namespace std
