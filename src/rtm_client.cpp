#include "rtm_client.h"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <deque>
#include <gsl/gsl>
#include <iostream>
#include <json.hpp>
#include <memory>
#include <utility>

#include "cbor_json.h"
#include "logging.h"
#include "metrics.h"
#include "threadutils.h"

namespace asio = boost::asio;

namespace satori {

namespace video {

namespace rtm {

using endpoint_iterator_t = asio::ip::tcp::resolver::iterator;
using endpoint_t = asio::ip::tcp::resolver::endpoint_type;

struct client_error_category : std::error_category {
  const char *name() const noexcept override { return "rtm-client"; }
  std::string message(int ev) const override {
    switch (static_cast<client_error>(ev)) {
      case client_error::UNKNOWN:
        return "unknown error";
      case client_error::NOT_CONNECTED:
        return "client is not connected";
      case client_error::RESPONSE_PARSING_ERROR:
        return "error parsing response";
      case client_error::INVALID_RESPONSE:
        return "invalid response";
      case client_error::SUBSCRIPTION_ERROR:
        return "subscription error";
      case client_error::SUBSCRIBE_ERROR:
        return "subscribe error";
      case client_error::UNSUBSCRIBE_ERROR:
        return "unsubscribe error";
      case client_error::ASIO_ERROR:
        return "asio error";
      case client_error::INVALID_MESSAGE:
        return "invalid message";
      case client_error::PUBLISH_ERROR:
        return "publish error";
    }
  }
};

std::error_condition make_error_condition(client_error e) {
  static client_error_category category;
  return {static_cast<int>(e), category};
}

namespace {

constexpr int read_buffer_size = 100000;
constexpr bool use_cbor = true;

const boost::posix_time::minutes ws_ping_interval{1};

auto &rtm_client_start = prometheus::BuildCounter()
                             .Name("rtm_client_start")
                             .Register(metrics_registry())
                             .Add({});

auto &rtm_client_error =
    prometheus::BuildCounter().Name("rtm_client_error").Register(metrics_registry());

auto &rtm_actions_received = prometheus::BuildCounter()
                                 .Name("rtm_actions_received_total")
                                 .Register(metrics_registry());

auto &rtm_messages_received = prometheus::BuildCounter()
                                  .Name("rtm_messages_received_total")
                                  .Register(metrics_registry());

auto &rtm_messages_bytes_received = prometheus::BuildCounter()
                                        .Name("rtm_messages_received_bytes_total")
                                        .Register(metrics_registry());

auto &rtm_messages_sent = prometheus::BuildCounter()
                              .Name("rtm_messages_sent_total")
                              .Register(metrics_registry());

auto &rtm_messages_bytes_sent = prometheus::BuildCounter()
                                    .Name("rtm_messages_sent_bytes_total")
                                    .Register(metrics_registry());

auto &rtm_messages_in_pdu =
    prometheus::BuildHistogram()
        .Name("rtm_messages_in_pdu")
        .Register(metrics_registry())
        .Add({}, std::vector<double>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 30, 40, 50, 60,
                                     70, 80, 90, 100});

auto &rtm_bytes_written = prometheus::BuildCounter()
                              .Name("rtm_bytes_written_total")
                              .Register(metrics_registry())
                              .Add({});

auto &rtm_bytes_read = prometheus::BuildCounter()
                           .Name("rtm_bytes_read_total")
                           .Register(metrics_registry())
                           .Add({});

auto &rtm_pings_sent_total = prometheus::BuildCounter()
                                 .Name("rtm_pings_sent_total")
                                 .Register(metrics_registry())
                                 .Add({});

auto &rtm_frames_received_total = prometheus::BuildCounter()
                                      .Name("rtm_frames_received_total")
                                      .Register(metrics_registry());

auto &rtm_last_pong_time_seconds = prometheus::BuildGauge()
                                       .Name("rtm_last_pong_time_seconds")
                                       .Register(metrics_registry())
                                       .Add({});

auto &rtm_last_ping_time_seconds = prometheus::BuildGauge()
                                       .Name("rtm_last_ping_time_seconds")
                                       .Register(metrics_registry())
                                       .Add({});

auto &rtm_subscription_error_total = prometheus::BuildCounter()
                                         .Name("rtm_subscription_error_total")
                                         .Register(metrics_registry())
                                         .Add({});

auto &rtm_publish_time_microseconds =
    prometheus::BuildHistogram()
        .Name("rtm_publish_time_microseconds")
        .Register(metrics_registry())
        .Add({}, std::vector<double>{0,    1,    5,     10,    25,    50,    100,
                                     250,  500,  750,   1000,  2000,  3000,  4000,
                                     5000, 7500, 10000, 25000, 50000, 100000});

auto &rtm_publish_ack_time_delta_millis_family =
    prometheus::BuildHistogram()
        .Name("rtm_publish_ack_time_delta_millis")
        .Register(metrics_registry())
        .Add({},
             std::vector<double>{0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,
                                 15,  20,  25,  30,  40,  50,  60,  70,  80,  90,  100,
                                 150, 200, 250, 300, 400, 500, 600, 700, 800, 900, 1000});

auto &rtm_publish_error_total = prometheus::BuildCounter()
                                    .Name("rtm_publish_error_total")
                                    .Register(metrics_registry())
                                    .Add({});

auto &rtm_publish_inflight_total = prometheus::BuildGauge()
                                       .Name("rtm_publish_inflight_total")
                                       .Register(metrics_registry())
                                       .Add({});

auto &rtm_subscribe_error_total = prometheus::BuildCounter()
                                      .Name("rtm_subscribe_error_total")
                                      .Register(metrics_registry())
                                      .Add({});

auto &rtm_unsubscribe_error_total = prometheus::BuildCounter()
                                        .Name("rtm_unsubscribe_error_total")
                                        .Register(metrics_registry())
                                        .Add({});

struct subscribe_request {
  const uint64_t id;
  const std::string channel;
  boost::optional<uint64_t> age;
  boost::optional<uint64_t> count;

  nlohmann::json to_json() const {
    nlohmann::json document =
        R"({"action":"rtm/subscribe", "body":{"channel":"<not_set>", "subscription_id":"<not_set>"}, "id": 2})"_json;

    CHECK(document.is_object());
    document["id"] = id;
    auto &body = document["body"];
    body["channel"] = channel;
    body["subscription_id"] = channel;

    if (age || count) {
      nlohmann::json history;
      if (age) {
        history.emplace("age", *age);
      }
      if (count) {
        history.emplace("count", *count);
      }

      body.emplace("history", history);
    }

    return document;
  }
};

struct unsubscribe_request {
  const uint64_t id;
  const std::string channel;

  nlohmann::json to_json() const {
    nlohmann::json document =
        R"({"action":"rtm/unsubscribe", "body":{"subscription_id":"<not_set>"}, "id": 2})"_json;

    CHECK(document.is_object());
    document["id"] = id;
    auto &body = document["body"];
    body["subscription_id"] = channel;

    return document;
  }
};

enum class subscription_status {
  PENDING_SUBSCRIBE = 1,
  CURRENT = 2,
  PENDING_UNSUBSCRIBE = 3
};

std::ostream &operator<<(std::ostream &out, subscription_status const &s) {
  switch (s) {
    case subscription_status::PENDING_SUBSCRIBE:
      return out << "PENDING_SUBSCRIBE";
    case subscription_status::CURRENT:
      return out << "CURRENT";
    case subscription_status::PENDING_UNSUBSCRIBE:
      return out << "PENDING_UNSUBSCRIBE";
  }
}

struct subscription_impl {
  const std::string channel;
  const subscription &sub;
  subscription_callbacks &callbacks;
  subscription_status status;
  uint64_t pending_request_id{UINT64_MAX};
};

enum class client_state { STOPPED = 1, RUNNING = 2, PENDING_STOPPED = 3 };

std::ostream &operator<<(std::ostream &out, client_state const &s) {
  switch (s) {
    case client_state::RUNNING:
      return out << "RUNNING";
    case client_state::PENDING_STOPPED:
      return out << "PENDING_STOPPED";
    case client_state::STOPPED:
      return out << "STOPPED";
  }
}

class secure_client : public client {
 public:
  explicit secure_client(const std::string &host, const std::string &port,
                         const std::string &appkey, uint64_t client_id,
                         error_callbacks &callbacks, asio::io_service &io_service,
                         asio::ssl::context &ssl_ctx)
      : _host{host},
        _port{port},
        _appkey{appkey},
        _tcp_resolver{io_service},
        _ws{io_service, ssl_ctx},
        _client_id{client_id},
        _callbacks{callbacks},
        _ping_timer{io_service} {
    _control_callback = [](boost::beast::websocket::frame_type type,
                           boost::beast::string_view payload) {
      switch (type) {
        case boost::beast::websocket::frame_type::close:
          rtm_frames_received_total.Add({{"type", "close"}}).Increment();
          LOG(2) << "got close frame " << payload;
          break;
        case boost::beast::websocket::frame_type::ping:
          rtm_frames_received_total.Add({{"type", "ping"}}).Increment();
          LOG(2) << "got ping frame " << payload;
          break;
        case boost::beast::websocket::frame_type::pong:
          rtm_frames_received_total.Add({{"type", "pong"}}).Increment();
          auto time_since_epoch = std::chrono::system_clock::now().time_since_epoch();
          rtm_last_pong_time_seconds.Set(
              std::chrono::duration_cast<std::chrono::seconds>(time_since_epoch).count());
          LOG(2) << "got pong frame " << payload;
          break;
      }
    };
  }

  ~secure_client() override = default;

  std::error_condition start() override {
    CHECK_EQ(_client_state.load(), client_state::STOPPED);
    LOG(INFO) << "Starting secure RTM client: " << _host << ":" << _port
              << ", appkey: " << _appkey;

    boost::system::error_code ec;

    auto endpoints = _tcp_resolver.resolve({_host, _port}, ec);
    if (ec.value() != 0) {
      LOG(ERROR) << "can't resolve endpoint: [" << ec << "] " << ec.message();
      rtm_client_error.Add({{"type", "tcp_resolve_endpoint"}}).Increment();
      return make_error_condition(client_error::ASIO_ERROR);
    }

    _ws.read_message_max(read_buffer_size);

    // tcp connect
    asio::connect(_ws.next_layer().next_layer(), endpoints, ec);
    if (ec.value() != 0) {
      LOG(ERROR) << "can't connect: [" << ec << "] " << ec.message();
      rtm_client_error.Add({{"type", "tcp_connect"}}).Increment();
      return make_error_condition(client_error::ASIO_ERROR);
    }

    // ssl handshake
    _ws.next_layer().handshake(boost::asio::ssl::stream_base::client, ec);
    if (ec.value() != 0) {
      LOG(ERROR) << "can't handshake SSL: [" << ec << "] " << ec.message();
      rtm_client_error.Add({{"type", "ssl_handshake"}}).Increment();
      return make_error_condition(client_error::ASIO_ERROR);
    }

    // upgrade to ws.
    boost::beast::websocket::response_type ws_upgrade_response;
    _ws.handshake_ex(ws_upgrade_response, _host + ":" + _port, "/v2?appkey=" + _appkey,
                     [this](boost::beast::websocket::request_type &ws_upgrade_request) {
                       if (use_cbor) {
                         ws_upgrade_request.set(
                             boost::beast::http::field::sec_websocket_protocol, "cbor");
                       }
                       LOG(2) << "websocket upgrade request:\n" << ws_upgrade_request;
                     },
                     ec);
    LOG(2) << "websocket upgrade response:\n" << ws_upgrade_response;
    if (ec.value() != 0) {
      LOG(ERROR) << "can't upgrade to websocket protocol: [" << ec << "] "
                 << ec.message();
      rtm_client_error.Add({{"type", "ws_upgrade"}}).Increment();
      return make_error_condition(client_error::ASIO_ERROR);
    }
    LOG(INFO) << "websocket open";
    rtm_client_start.Increment();

    _ws.control_callback(_control_callback);
    if (use_cbor) {
      _ws.binary(true);
    }

    _last_ping_time = std::chrono::system_clock::now();
    arm_ping_timer();

    _client_state = client_state::RUNNING;
    ask_for_read();
    return {};
  }

  std::error_condition stop() override {
    CHECK_EQ(_client_state, client_state::RUNNING);
    LOG(INFO) << "Stopping secure RTM client";

    _client_state = client_state::PENDING_STOPPED;
    boost::system::error_code ec;
    _ping_timer.cancel(ec);
    if (ec.value() != 0) {
      LOG(ERROR) << "can't stop ping timer: [" << ec << "] " << ec.message();
      rtm_client_error.Add({{"type", "stop_ping_timer"}}).Increment();
      return make_error_condition(client_error::ASIO_ERROR);
    }

    _ws.next_layer().next_layer().close(ec);
    if (ec.value() != 0) {
      LOG(ERROR) << "can't close: [" << ec << "] " << ec.message();
      rtm_client_error.Add({{"type", "close_connection"}}).Increment();
      return make_error_condition(client_error::ASIO_ERROR);
    }

    _ws.control_callback();
    return {};
  }

  void publish(const std::string &channel, nlohmann::json &&message,
               publish_callbacks *callbacks) override {
    CHECK(!callbacks) << "not implemented";
    if (_client_state == client_state::PENDING_STOPPED) {
      LOG(1) << "RTM client is pending stop";
      return;
    }
    CHECK_EQ(_client_state, client_state::RUNNING) << "RTM client is not running";

    nlohmann::json document =
        R"({"action":"rtm/publish", "body":{"channel":"<not_set>", "message":"<not_set>"}, "id":"<not_set>"})"_json;

    CHECK(document.is_object());

    auto &body = document["body"];
    body["channel"] = channel;
    body["message"] = message;

    const uint64_t request_id = ++_request_id;
    document["id"] = request_id;

    const std::string buffer = use_cbor ? json_to_cbor(document) : document.dump();

    rtm_messages_sent.Add({{"channel", channel}}).Increment();
    rtm_messages_bytes_sent.Add({{"channel", channel}}).Increment(buffer.size());
    rtm_bytes_written.Increment(buffer.size());

    boost::system::error_code ec;
    const auto before_publish = std::chrono::system_clock::now();
    _ws.write(asio::buffer(buffer), ec);
    const auto after_publish = std::chrono::system_clock::now();
    _publish_times.emplace(request_id, before_publish);
    rtm_publish_time_microseconds.Observe(
        std::chrono::duration_cast<std::chrono::microseconds>(after_publish
                                                              - before_publish)
            .count());
    if (ec.value() != 0) {
      LOG(ERROR) << "publish request failure: [" << ec << "] " << ec.message();
      rtm_client_error.Add({{"type", "publish"}}).Increment();
    }
  }

  void subscribe_channel(const std::string &channel, const subscription &sub,
                         subscription_callbacks &callbacks,
                         const subscription_options *options) override {
    if (_client_state == client_state::PENDING_STOPPED) {
      LOG(1) << "RTM client is pending stop";
      return;
    }
    CHECK_EQ(_client_state, client_state::RUNNING) << "RTM client is not running";

    _subscriptions.emplace(std::make_pair(
        channel,
        subscription_impl{channel, sub, callbacks, subscription_status::PENDING_SUBSCRIBE,
                          ++_request_id}));

    subscribe_request request{_request_id, channel};
    if (options != nullptr) {
      request.age = options->history.age;
      request.count = options->history.count;
    }

    nlohmann::json document = request.to_json();
    const std::string buffer = use_cbor ? json_to_cbor(document) : document.dump();

    rtm_bytes_written.Increment(buffer.size());
    boost::system::error_code ec;
    _ws.write(asio::buffer(buffer), ec);
    if (ec.value() != 0) {
      LOG(ERROR) << "subscribe request failure: [" << ec << "] " << ec.message();
      rtm_client_error.Add({{"type", "subscribe"}}).Increment();
    }

    LOG(1) << "requested subscribe: " << document;
  }

  void subscribe_filter(const std::string & /*filter*/, const subscription & /*sub*/,
                        subscription_callbacks & /*callbacks*/,
                        const subscription_options * /*options*/) override {
    ABORT() << "NOT IMPLEMENTED";
  }

  void unsubscribe(const subscription &sub_to_delete) override {
    if (_client_state == client_state::PENDING_STOPPED) {
      LOG(1) << "RTM client is pending stop";
      return;
    }
    CHECK_EQ(_client_state, client_state::RUNNING) << "RTM client is not running";

    for (auto &it : _subscriptions) {
      const std::string &sub_id = it.first;
      subscription_impl &sub = it.second;
      if (&sub.sub != &sub_to_delete) {
        continue;
      }

      unsubscribe_request request{++_request_id, sub_id};
      nlohmann::json document = request.to_json();
      const std::string buffer = use_cbor ? json_to_cbor(document) : document.dump();

      rtm_bytes_written.Increment(buffer.size());

      boost::system::error_code ec;
      _ws.write(asio::buffer(buffer), ec);
      if (ec.value() != 0) {
        LOG(ERROR) << "unsubscribe request failure: [" << ec << "] " << ec.message();
        rtm_client_error.Add({{"type", "unsubscribe"}}).Increment();
      }

      sub.pending_request_id = _request_id;
      sub.status = subscription_status::PENDING_UNSUBSCRIBE;

      LOG(1) << "requested unsubscribe: " << document;
      return;
    }
    ABORT() << "didn't find subscription";
  }

  channel_position position(const subscription & /*sub*/) override {
    ABORT() << "NOT IMPLEMENTED";
    return {0, 0};
  }

  bool is_up(const subscription & /*sub*/) override {
    ABORT() << "NOT IMPLEMENTED";
    return false;
  }

 private:
  void ask_for_read() {
    _ws.async_read(_read_buffer, [this](boost::system::error_code const &ec,
                                        unsigned long) {
      const auto arrival_time = std::chrono::system_clock::now();

      LOG(2) << this << " async_read ec=" << ec;
      if (ec.value() != 0) {
        if (ec == boost::asio::error::operation_aborted) {
          LOG(INFO) << this << " async_read operation is cancelled";
          CHECK(_client_state == client_state::PENDING_STOPPED);
          _client_state = client_state::STOPPED;
          _subscriptions.clear();
        } else if (_client_state == client_state::RUNNING) {
          rtm_client_error.Add({{"type", "read"}}).Increment();
          LOG(ERROR) << this << " asio error: [" << ec << "] " << ec.message();
          _callbacks.on_error(client_error::ASIO_ERROR);
        } else {
          LOG(INFO) << this << " ignoring asio error because in state " << _client_state
                    << ": [" << ec << "] " << ec.message();
        }
        return;
      }

      const std::string buffer = boost::beast::buffers_to_string(_read_buffer.data());
      CHECK_EQ(buffer.size(), _read_buffer.size());
      _read_buffer.consume(_read_buffer.size());
      rtm_bytes_read.Increment(_read_buffer.size());

      nlohmann::json document;

      if (use_cbor) {
        auto doc_or_error = cbor_to_json(buffer);
        if (!doc_or_error.ok()) {
          LOG(ERROR) << "CBOR message couldn't be processed: "
                     << doc_or_error.error_message();
          return;
        }
        document = doc_or_error.get();
      } else {
        try {
          document = nlohmann::json::parse(buffer);
        } catch (const std::exception &e) {
          LOG(ERROR) << "Bad data: " << e.what() << " " << buffer;
          return;
        }
      }

      LOG(9) << this << " async_read processing input";
      process_input(document, buffer.size(), arrival_time);

      LOG(9) << this << " async_read asking for read";
      ask_for_read();
    });
  }

  void arm_ping_timer() {
    LOG(2) << this << " setting ws ping timer";

    _ping_timer.expires_from_now(ws_ping_interval);
    _ping_timer.async_wait([this](const boost::system::error_code &ec_timer) {
      LOG(2) << this << " ping timer ec=" << ec_timer;
      if (ec_timer.value() != 0) {
        if (ec_timer == boost::asio::error::operation_aborted) {
          LOG(INFO) << this << " ping timer is cancelled";
        } else if (_client_state == client_state::RUNNING) {
          LOG(ERROR) << this << " ping timer error for ping timer: [" << ec_timer << "] "
                     << ec_timer.message();
          rtm_client_error.Add({{"type", "ping_timer"}}).Increment();
          _callbacks.on_error(client_error::ASIO_ERROR);
        } else {
          LOG(INFO) << this << " ignoring asio error for ping timer because in state "
                    << _client_state << ": [" << ec_timer << "] " << ec_timer.message();
        }
        return;
      }

      _ws.async_ping("pingmsg", [this](boost::system::error_code const &ec_ping) {
        LOG(2) << this << " ping write ec=" << ec_ping;
        if (ec_ping.value() != 0) {
          if (ec_ping == boost::asio::error::operation_aborted) {
            LOG(INFO) << this << " ping operation is cancelled";
          } else if (_client_state == client_state::RUNNING) {
            LOG(ERROR) << this << " asio error for ping operation: [" << ec_ping << "] "
                       << ec_ping.message();
            rtm_client_error.Add({{"type", "ping"}}).Increment();
            _callbacks.on_error(client_error::ASIO_ERROR);
          } else {
            LOG(INFO) << this
                      << " ignoring asio error for ping operation because in state "
                      << _client_state << ": [" << ec_ping << "] " << ec_ping.message();
          }
          return;
        }

        rtm_pings_sent_total.Increment();
        const auto now = std::chrono::system_clock::now();
        rtm_last_ping_time_seconds.Set(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - _last_ping_time)
                .count()
            / 1000.0);
        _last_ping_time = now;

        LOG(2) << this << " scheduling next ping";
        arm_ping_timer();
      });
    });
  }

  void process_publish_confirmation(
      const nlohmann::json &pdu,
      const std::chrono::system_clock::time_point arrival_time) {
    CHECK(pdu.find("id") != pdu.end()) << "no id in pdu: " << pdu;
    const uint64_t id = pdu["id"];
    auto it = _publish_times.find(id);
    CHECK(it != _publish_times.end()) << "unexpected publish confirmation: " << pdu;
    const std::chrono::system_clock::time_point publish_time = it->second;
    _publish_times.erase(it);
    const double delta = std::abs(
        std::chrono::duration_cast<std::chrono::milliseconds>(arrival_time - publish_time)
            .count());
    rtm_publish_ack_time_delta_millis_family.Observe(delta);
    rtm_publish_inflight_total.Set(_publish_times.size());
  }

  void process_input(const nlohmann::json &pdu, size_t byte_size,
                     std::chrono::system_clock::time_point arrival_time) {
    CHECK(pdu.is_object()) << "not an object: " << pdu;

    CHECK(pdu.find("action") != pdu.end()) << "no action in pdu: " << pdu;

    const std::string action = pdu["action"];
    rtm_actions_received.Add({{"action", action}}).Increment();

    if (action == "rtm/subscription/data") {
      CHECK(pdu.find("body") != pdu.end()) << "no body in pdu: " << pdu;
      auto &body = pdu["body"];

      CHECK(body.find("subscription_id") != body.end())
          << "no subscription_id in body: " << pdu;
      std::string subscription_id = body["subscription_id"];
      auto it = _subscriptions.find(subscription_id);
      CHECK(it != _subscriptions.end());
      subscription_impl &sub = it->second;
      CHECK(sub.status == subscription_status::CURRENT
            || sub.status == subscription_status::PENDING_UNSUBSCRIBE);
      if (sub.status == subscription_status::PENDING_UNSUBSCRIBE) {
        LOG(2) << "Got data for subscription pending deletion";
        return;
      }

      CHECK(body.find("messages") != body.end()) << "no messages in body: " << pdu;
      auto &messages = body["messages"];
      CHECK(messages.is_array()) << "messages is not an array: " << pdu;

      rtm_messages_received.Add({{"channel", sub.channel}}).Increment();
      rtm_messages_bytes_received.Add({{"channel", sub.channel}}).Increment(byte_size);
      rtm_messages_in_pdu.Observe(messages.size());

      for (const auto &m : messages) {
        // TODO: eliminate copies of m
        channel_data data{m, arrival_time};
        sub.callbacks.on_data(sub.sub, std::move(data));
      }
    } else if (action == "rtm/subscription/error") {
      LOG(ERROR) << "subscription error: " << pdu;
      rtm_subscription_error_total.Increment();
      _callbacks.on_error(client_error::SUBSCRIPTION_ERROR);
    } else if (action == "rtm/publish/ok") {
      process_publish_confirmation(pdu, arrival_time);
    } else if (action == "rtm/publish/error") {
      process_publish_confirmation(pdu, arrival_time);
      LOG(ERROR) << "got publish error: " << pdu;
      rtm_publish_error_total.Increment();
      _callbacks.on_error(client_error::PUBLISH_ERROR);
    } else if (action == "rtm/subscribe/ok") {
      CHECK(pdu.find("id") != pdu.end()) << "no id in pdu: " << pdu;
      const uint64_t id = pdu["id"];
      for (auto &it : _subscriptions) {
        const std::string &sub_id = it.first;
        subscription_impl &sub = it.second;
        if (sub.pending_request_id == id) {
          LOG(1) << "got subscribe confirmation for subscription " << sub_id
                 << " in status " << sub.status << ": " << pdu;
          CHECK(sub.status == subscription_status::PENDING_SUBSCRIBE);
          sub.pending_request_id = UINT64_MAX;
          sub.status = subscription_status::CURRENT;
          return;
        }
      }
      ABORT() << "got unexpected subscribe confirmation: " << pdu;
    } else if (action == "rtm/subscribe/error") {
      CHECK(pdu.find("id") != pdu.end()) << "no id in pdu: " << pdu;
      const uint64_t id = pdu["id"];
      for (auto it = _subscriptions.begin(); it != _subscriptions.end(); ++it) {
        const std::string &sub_id = it->first;
        subscription_impl &sub = it->second;
        if (sub.pending_request_id == id) {
          LOG(ERROR) << "got subscribe error for subscription " << sub_id << " in status "
                     << sub.status << ": " << pdu;
          CHECK(sub.status == subscription_status::PENDING_SUBSCRIBE);
          rtm_subscribe_error_total.Increment();
          _callbacks.on_error(client_error::SUBSCRIBE_ERROR);
          _subscriptions.erase(it);
          return;
        }
      }
      ABORT() << "got unexpected subscribe error: " << pdu;
    } else if (action == "rtm/unsubscribe/ok") {
      CHECK(pdu.find("id") != pdu.end()) << "no id in pdu: " << pdu;
      const uint64_t id = pdu["id"];
      for (auto it = _subscriptions.begin(); it != _subscriptions.end(); ++it) {
        const std::string &sub_id = it->first;
        subscription_impl &sub = it->second;
        if (sub.pending_request_id == id) {
          LOG(1) << "got unsubscribe confirmation for subscription " << sub_id
                 << " in status " << sub.status << ": " << pdu;
          CHECK(sub.status == subscription_status::PENDING_UNSUBSCRIBE);
          it = _subscriptions.erase(it);
          return;
        }
      }
      ABORT() << "got unexpected unsubscribe confirmation: " << pdu;
    } else if (action == "rtm/unsubscribe/error") {
      CHECK(pdu.find("id") != pdu.end()) << "no id in pdu: " << pdu;
      const uint64_t id = pdu["id"];  // check type
      for (auto it = _subscriptions.begin(); it != _subscriptions.end(); ++it) {
        const std::string &sub_id = it->first;
        subscription_impl &sub = it->second;
        if (sub.pending_request_id == id) {
          LOG(ERROR) << "got unsubscribe error for subscription " << sub_id
                     << " in status " << sub.status << ": " << pdu;
          CHECK(sub.status == subscription_status::PENDING_UNSUBSCRIBE);
          rtm_unsubscribe_error_total.Increment();
          _callbacks.on_error(client_error::UNSUBSCRIBE_ERROR);
          _subscriptions.erase(it);
          return;
        }
      }
      ABORT() << "got unexpected unsubscribe error: " << pdu;
    } else if (action == "/error") {
      ABORT() << "got unexpected error: " << pdu;
    } else {
      ABORT() << "unsupported action: " << pdu;
    }
  }

  std::atomic<client_state> _client_state{client_state::STOPPED};

  const std::string _host;
  const std::string _port;
  const std::string _appkey;
  const uint64_t _client_id;
  error_callbacks &_callbacks;

  asio::ip::tcp::resolver _tcp_resolver;
  boost::beast::websocket::stream<boost::asio::ssl::stream<boost::asio::ip::tcp::socket> >
      _ws;
  uint64_t _request_id{0};
  boost::beast::multi_buffer _read_buffer{read_buffer_size};
  std::map<std::string, subscription_impl> _subscriptions;
  std::map<uint64_t, std::chrono::system_clock::time_point> _publish_times;
  boost::asio::deadline_timer _ping_timer;
  std::chrono::system_clock::time_point _last_ping_time;
  std::function<void(boost::beast::websocket::frame_type type,
                     boost::beast::string_view payload)>
      _control_callback;
};

}  // namespace

std::unique_ptr<client> new_client(const std::string &endpoint, const std::string &port,
                                   const std::string &appkey,
                                   asio::io_service &io_service,
                                   asio::ssl::context &ssl_ctx, size_t id,
                                   error_callbacks &callbacks) {
  LOG(1) << "Creating RTM client for " << endpoint << ":" << port << "?appkey=" << appkey;
  std::unique_ptr<secure_client> client(
      new secure_client(endpoint, port, appkey, id, callbacks, io_service, ssl_ctx));
  return std::move(client);
}

resilient_client::resilient_client(asio::io_service &io_service,
                                   std::thread::id io_thread_id,
                                   resilient_client::client_factory_t &&factory,
                                   error_callbacks &callbacks)
    : _io(io_service),
      _io_thread_id(io_thread_id),
      _factory(std::move(factory)),
      _error_callbacks(callbacks) {}

void resilient_client::publish(const std::string &channel, nlohmann::json &&message,
                               publish_callbacks *callbacks) {
  CHECK_EQ(std::this_thread::get_id(), _io_thread_id)
      << "Invocation from " << threadutils::get_current_thread_name();

  _client->publish(channel, std::move(message), callbacks);
}

void resilient_client::subscribe_channel(const std::string &channel,
                                         const subscription &sub,
                                         subscription_callbacks &callbacks,
                                         const subscription_options *options) {
  CHECK_EQ(std::this_thread::get_id(), _io_thread_id)
      << "Invocation from " << threadutils::get_current_thread_name();

  _subscriptions.push_back({channel, &sub, &callbacks, options});
  _client->subscribe_channel(channel, sub, callbacks, options);
}

void resilient_client::subscribe_filter(const std::string & /*filter*/,
                                        const subscription & /*sub*/,
                                        subscription_callbacks & /*callbacks*/,
                                        const subscription_options * /*options*/) {
  CHECK_EQ(std::this_thread::get_id(), _io_thread_id)
      << "Invocation from " << threadutils::get_current_thread_name();

  ABORT() << "not implemented";
}

void resilient_client::unsubscribe(const subscription &sub) {
  CHECK_EQ(std::this_thread::get_id(), _io_thread_id)
      << "Invocation from " << threadutils::get_current_thread_name();

  _client->unsubscribe(sub);
  std::remove_if(_subscriptions.begin(), _subscriptions.end(),
                 [&sub](const subscription_info &si) { return &sub == si.sub; });
}

channel_position resilient_client::position(const subscription &sub) {
  CHECK_EQ(std::this_thread::get_id(), _io_thread_id)
      << "Invocation from " << threadutils::get_current_thread_name();

  return _client->position(sub);
}

bool resilient_client::is_up(const subscription &sub) {
  CHECK_EQ(std::this_thread::get_id(), _io_thread_id)
      << "Invocation from " << threadutils::get_current_thread_name();

  return _client->is_up(sub);
}

std::error_condition resilient_client::start() {
  CHECK_EQ(std::this_thread::get_id(), _io_thread_id)
      << "Invocation from " << threadutils::get_current_thread_name();

  if (!_client) {
    LOG(1) << "creating new client";
    _client = _factory(*this);
  }

  _started = true;
  return _client->start();
}

std::error_condition resilient_client::stop() {
  CHECK_EQ(std::this_thread::get_id(), _io_thread_id)
      << "Invocation from " << threadutils::get_current_thread_name();

  _started = false;
  return _client->stop();
}

void resilient_client::on_error(std::error_condition ec) {
  CHECK_EQ(std::this_thread::get_id(), _io_thread_id)
      << "Invocation from " << threadutils::get_current_thread_name();

  LOG(INFO) << "restarting rtm client because of error: " << ec.message();
  restart();
}

void resilient_client::restart() {
  CHECK_EQ(std::this_thread::get_id(), _io_thread_id)
      << "Invocation from " << threadutils::get_current_thread_name();

  LOG(1) << "creating new client";
  _client = _factory(*this);
  if (!_started) {
    return;
  }

  LOG(1) << "starting new client";
  auto ec = _client->start();
  if (ec) {
    LOG(ERROR) << "can't restart client: " << ec.message();
    _error_callbacks.on_error(ec);
    return;
  }

  LOG(1) << "restoring subscriptions";
  for (const auto &sub : _subscriptions) {
    _client->subscribe_channel(sub.channel, *sub.sub, *sub.callbacks, sub.options);
  }

  LOG(1) << "client restart done";
}

thread_checking_client::thread_checking_client(asio::io_service &io,
                                               std::thread::id io_thread_id,
                                               std::unique_ptr<client> client)
    : _io(io), _io_thread_id(io_thread_id), _client(std::move(client)) {}

void thread_checking_client::publish(const std::string &channel, nlohmann::json &&message,
                                     publish_callbacks *callbacks) {
  if (std::this_thread::get_id() != _io_thread_id) {
    LOG(WARNING) << "Forwarding request from thread "
                 << threadutils::get_current_thread_name();
    _io.post([ this, channel, message = std::move(message), callbacks ]() mutable {
      _client->publish(channel, std::move(message), callbacks);
    });
    return;
  }

  _client->publish(channel, std::move(message), callbacks);
}

void thread_checking_client::subscribe_channel(const std::string &channel,
                                               const subscription &sub,
                                               subscription_callbacks &callbacks,
                                               const subscription_options *options) {
  if (std::this_thread::get_id() != _io_thread_id) {
    LOG(WARNING) << "Forwarding request from thread "
                 << threadutils::get_current_thread_name();
    _io.post([this, channel, &sub, &callbacks, options]() {
      _client->subscribe_channel(channel, sub, callbacks, options);
    });
    return;
  }

  _client->subscribe_channel(channel, sub, callbacks, options);
}

void thread_checking_client::subscribe_filter(const std::string &filter,
                                              const subscription &sub,
                                              subscription_callbacks &callbacks,
                                              const subscription_options *options) {
  if (std::this_thread::get_id() != _io_thread_id) {
    LOG(WARNING) << "Forwarding request from thread "
                 << threadutils::get_current_thread_name();
    _io.post([this, filter, &sub, &callbacks, options]() {
      _client->subscribe_filter(filter, sub, callbacks, options);
    });
    return;
  }

  _client->subscribe_filter(filter, sub, callbacks, options);
}

void thread_checking_client::unsubscribe(const subscription &sub) {
  if (std::this_thread::get_id() != _io_thread_id) {
    LOG(5) << "Forwarding request from thread " << threadutils::get_current_thread_name();
    _io.post([this, &sub]() { _client->unsubscribe(sub); });
    return;
  }

  _client->unsubscribe(sub);
}

channel_position thread_checking_client::position(const subscription & /*sub*/) {
  ABORT() << "not implemented";
  return channel_position{0, 0};
}

bool thread_checking_client::is_up(const subscription & /*sub*/) {
  ABORT() << "not implemented";
  return false;
}

std::error_condition thread_checking_client::start() {
  CHECK_EQ(std::this_thread::get_id(), _io_thread_id)
      << "Invocation from " << threadutils::get_current_thread_name();

  return _client->start();
}

std::error_condition thread_checking_client::stop() {
  CHECK_EQ(std::this_thread::get_id(), _io_thread_id)
      << "Invocation from " << threadutils::get_current_thread_name();

  return _client->stop();
}

}  // namespace rtm
}  // namespace video
}  // namespace satori
