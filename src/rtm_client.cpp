#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <deque>
#include <iostream>
#include <memory>
#include <utility>

#include "cbor_json.h"
#include "logging.h"
#include "rtm_client.h"

namespace asio = boost::asio;

namespace rtm {

using endpoint_iterator_t = asio::ip::tcp::resolver::iterator;
using endpoint_t = asio::ip::tcp::resolver::endpoint_type;

struct client_error_category : std::error_category {
  const char *name() const noexcept override { return "rtm-client"; }
  std::string message(int ev) const override {
    switch (static_cast<client_error>(ev)) {
      case client_error::Unknown:
        return "unknown error";
      case client_error::NotConnected:
        return "client is not connected";
      case client_error::ResponseParsingError:
        return "error parsing response";
      case client_error::InvalidResponse:
        return "invalid response";
      case client_error::SubscriptionError:
        return "subscription error";
      case client_error::SubscribeError:
        return "subscribe error";
      case client_error::UnsubscribeError:
        return "unsubscribe error";
      case client_error::AsioError:
        return "asio error";
    }
  }
};

std::error_condition make_error_condition(client_error e) {
  static client_error_category category;
  return std::error_condition(static_cast<int>(e), category);
}

namespace {

constexpr int READ_BUFFER_SIZE = 100000;

static std::string to_string(const rapidjson::Value &d) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  d.Accept(writer);
  return std::string(buffer.GetString());
}

struct subscribe_request {
  const uint64_t id;
  const std::string channel;
  boost::optional<uint64_t> age;
  boost::optional<uint64_t> count;

  void serialize_to(rapidjson::Document &document) const {
    constexpr const char *tmpl =
        R"({"action":"rtm/subscribe",
            "body":{"channel":"<not_set>",
                    "subscription_id":"<not_set>"},
            "id": 2})";
    document.Parse(tmpl);
    CHECK(document.IsObject());
    document["id"].SetInt64(id);
    auto body = document["body"].GetObject();
    body["channel"].SetString(channel.c_str(), channel.length(), document.GetAllocator());
    body["subscription_id"].SetString(channel.c_str(), channel.length(),
                                      document.GetAllocator());

    if (age || count) {
      rapidjson::Value history(rapidjson::kObjectType);
      if (age) history.AddMember("age", *age, document.GetAllocator());
      if (count) history.AddMember("count", *count, document.GetAllocator());

      body.AddMember("history", history, document.GetAllocator());
    }
  }
};

struct unsubscribe_request {
  const uint64_t id;
  const std::string channel;

  void serialize_to(rapidjson::Document &document) const {
    constexpr const char *tmpl =
        R"({"action":"rtm/unsubscribe",
            "body":{"subscription_id":"<not_set>"},
            "id": 2})";
    document.Parse(tmpl);
    CHECK(document.IsObject());
    document["id"].SetInt64(id);
    auto body = document["body"].GetObject();
    body["subscription_id"].SetString(channel.c_str(), channel.length(),
                                      document.GetAllocator());
  }
};

enum class subscription_status {
  PendingSubscribe = 1,
  Current = 2,
  PendingUnsubscribe = 3
};

struct subscription_impl {
  const subscription &sub;
  subscription_callbacks &callbacks;
  subscription_status status;
  uint64_t pending_request_id{UINT64_MAX};
};

class secure_client : public client {
 public:
  explicit secure_client(std::string host, std::string port, std::string appkey,
                         uint64_t client_id, error_callbacks &callbacks,
                         asio::io_service &io_service, asio::ssl::context &ssl_ctx)
      : _host(host),
        _port(port),
        _appkey(appkey),
        _tcp_resolver{io_service},
        _ws{io_service, ssl_ctx},
        _client_id(client_id),
        _callbacks(callbacks) {}

  ~secure_client() override = default;

  std::error_condition start() override {
    CHECK(_client_state.load() == client_state::Stopped);
    LOG(INFO) << "Starting secure RTM client: " << _host << ":" << _port
              << ", appkey: " << _appkey;

    boost::system::error_code ec;

    auto endpoints = _tcp_resolver.resolve({_host, _port}, ec);
    if (ec) {
      LOG(ERROR) << "can't resolve endpoint: " << ec.message();
      return make_error_condition(client_error::AsioError);
    }

    _ws.read_message_max(READ_BUFFER_SIZE);

    // tcp connect
    asio::connect(_ws.next_layer().next_layer(), endpoints, ec);
    if (ec) {
      LOG(ERROR) << "can't connect: " << ec.message();
      return make_error_condition(client_error::AsioError);
    }

    // ssl handshake
    _ws.next_layer().handshake(boost::asio::ssl::stream_base::client);

    // upgrade to ws.
    _ws.handshake(_host, "/v2?appkey=" + _appkey, ec);
    if (ec) {
      LOG(ERROR) << "can't upgrade to websocket protocol: " << ec.message();
      return make_error_condition(client_error::AsioError);
    }
    LOG(1) << "Websocket open";

    _client_state = client_state::Running;
    ask_for_read();
    return {};
  }

  std::error_condition stop() override {
    CHECK(_client_state == client_state::Running);
    LOG(INFO) << "Stopping secure RTM client";

    _client_state = client_state::PendingStopped;
    boost::system::error_code ec;
    _ws.next_layer().next_layer().close(ec);
    if (ec)  {
          LOG(ERROR) << "can't close: " << ec.message();
      return make_error_condition(client_error::AsioError);

    }
    return {};
  }

  void publish(const std::string &channel, const cbor_item_t *message,
               publish_callbacks *callbacks) override {
    CHECK(!callbacks) << "not implemeneted";
    CHECK(_client_state == client_state::Running) << "Secure RTM client is not running";
    rapidjson::Document document;
    constexpr const char *tmpl =
        R"({"action":"rtm/publish",
            "body":{"channel":"<not_set>"}})";
    document.Parse(tmpl);
    CHECK(document.IsObject());
    auto body = document["body"].GetObject();
    body["channel"].SetString(channel.c_str(), channel.length(), document.GetAllocator());

    body.AddMember("message", video::cbor_to_json(message, document),
                   document.GetAllocator());

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    document.Accept(writer);
    _ws.write(asio::buffer(buf.GetString(), buf.GetSize()));
  }

  void subscribe_channel(const std::string &channel, const subscription &sub,
                         subscription_callbacks &callbacks,
                         const subscription_options *options) override {
    CHECK(_client_state == client_state::Running) << "Secure RTM client is not running";
    _subscriptions.emplace(std::make_pair(
        channel, subscription_impl{sub, callbacks, subscription_status::PendingSubscribe,
                                   ++_request_id}));

    rapidjson::Document document;
    subscribe_request req{_request_id, channel};
    if (options) {
      req.age = options->history.age;
      req.count = options->history.count;
    }
    req.serialize_to(document);
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    document.Accept(writer);
    _ws.write(asio::buffer(buf.GetString(), buf.GetSize()));
    LOG(1) << "requested subscribe for " << channel << ": "
              << std::string(buf.GetString());
  }

  void subscribe_filter(const std::string &filter, const subscription &sub,
                        subscription_callbacks &callbacks,
                        const subscription_options *options) override {
    ABORT() << "NOT IMPLEMENTED";
  }

  void unsubscribe(const subscription &sub_to_delete) override {
    CHECK(_client_state == client_state::Running) << "Secure RTM client is not running";
    for (auto it = _subscriptions.begin(); it != _subscriptions.end(); ++it) {
      const std::string &sub_id = it->first;
      subscription_impl &sub = it->second;
      if (&sub.sub != &sub_to_delete) continue;

      rapidjson::Document document;
      unsubscribe_request req{++_request_id, sub_id};
      req.serialize_to(document);
      rapidjson::StringBuffer buf;
      rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
      document.Accept(writer);
      _ws.write(asio::buffer(buf.GetString(), buf.GetSize()));
      sub.pending_request_id = _request_id;
      sub.status = subscription_status::PendingUnsubscribe;
      LOG(1) << "requested unsubscribe for " << sub_id << ": "
                << std::string(buf.GetString());
      return;
    }
    ABORT() << "didn't find subscription";
  }

  channel_position position(const subscription &sub) override {
    ABORT() << "NOT IMPLEMENTED";
    return {0, 0};
  }

  bool is_up(const subscription &sub) override {
    ABORT() << "NOT IMPLEMENTED";
    return false;
  }

 private:
  void ask_for_read() {
    _ws.async_read(_read_buffer, [this](boost::system::error_code const &ec,
                                        unsigned long) {
      if (ec == boost::asio::error::operation_aborted) {
        CHECK(_client_state == client_state::PendingStopped);
        LOG(INFO) << "Got stop request for async_read loop";
        _client_state = client_state::Stopped;
        _subscriptions.clear();
        return;
      }
      if (ec) {
        LOG(ERROR) << "asio error: " << ec.message();
        _callbacks.on_error(client_error::AsioError);
        return;
      }

      std::string input = boost::lexical_cast<std::string>(buffers(_read_buffer.data()));
      _read_buffer.consume(_read_buffer.size());

      rapidjson::StringStream s(input.c_str());
      rapidjson::Document d;
      d.ParseStream(s);
      process_input(d);

      ask_for_read();
    });
  }

  void process_input(rapidjson::Document &d) {
    if (!d.HasMember("action")) {
      std::cerr << "no action in pdu: " << to_string(d) << "\n";
    }

    std::string action = d["action"].GetString();
    if (action == "rtm/subscription/data") {
      auto body = d["body"].GetObject();
      std::string subscription_id = body["subscription_id"].GetString();
      auto it = _subscriptions.find(subscription_id);
      CHECK(it != _subscriptions.end());
      subscription_impl &sub = it->second;
      CHECK(sub.status == subscription_status::Current
            || sub.status == subscription_status::PendingUnsubscribe);
      if (sub.status == subscription_status::PendingUnsubscribe) {
        LOG(2) << "Got data for subscription pending deletion";
        return;
      }

      for (rapidjson::Value &m : body["messages"].GetArray()) {
        sub.callbacks.on_data(sub.sub, video::json_to_cbor(m));
      }
    } else if (action == "rtm/subscribe/ok") {
      const uint64_t id = d["id"].GetInt64();
      for (auto it = _subscriptions.begin(); it != _subscriptions.end(); ++it) {
        const std::string &sub_id = it->first;
        subscription_impl &sub = it->second;
        if (sub.pending_request_id == id) {
          LOG(1) << "got subscribe confirmation for subscription " << sub_id
                 << " in status " << std::to_string((int)sub.status) << ": "
                 << to_string(d);
          CHECK(sub.status == subscription_status::PendingSubscribe);
          sub.pending_request_id = UINT64_MAX;
          sub.status = subscription_status::Current;
          return;
        }
      }
      ABORT() << "got unexpected subscribe confirmation: " << to_string(d);
    } else if (action == "rtm/subscribe/error") {
      const uint64_t id = d["id"].GetInt64();
      for (auto it = _subscriptions.begin(); it != _subscriptions.end(); ++it) {
        const std::string &sub_id = it->first;
        subscription_impl &sub = it->second;
        if (sub.pending_request_id == id) {
          LOG(ERROR) << "got subscribe error for subscription " << sub_id << " in status "
                     << std::to_string((int)sub.status) << ": " << to_string(d);
          CHECK(sub.status == subscription_status::PendingSubscribe);
          _callbacks.on_error(client_error::SubscribeError);
          _subscriptions.erase(it);
          return;
        }
      }
      ABORT() << "got unexpected subscribe error: " << to_string(d);
    } else if (action == "rtm/unsubscribe/ok") {
      const uint64_t id = d["id"].GetInt64();
      for (auto it = _subscriptions.begin(); it != _subscriptions.end(); ++it) {
        const std::string &sub_id = it->first;
        subscription_impl &sub = it->second;
        if (sub.pending_request_id == id) {
          LOG(1) << "got unsubscribe confirmation for subscription " << sub_id
                    << " in status " << std::to_string((int)sub.status) << ": "
                    << to_string(d);
          CHECK(sub.status == subscription_status::PendingUnsubscribe);
          it = _subscriptions.erase(it);
          return;
        }
      }
      ABORT() << "got unexpected unsubscribe confirmation: " << to_string(d);
    } else if (action == "rtm/unsubscribe/error") {
      const uint64_t id = d["id"].GetInt64();
      for (auto it = _subscriptions.begin(); it != _subscriptions.end(); ++it) {
        const std::string &sub_id = it->first;
        subscription_impl &sub = it->second;
        if (sub.pending_request_id == id) {
          LOG(ERROR) << "got unsubscribe error for subscription " << sub_id
                     << " in status " << std::to_string((int)sub.status) << ": "
                     << to_string(d);
          CHECK(sub.status == subscription_status::PendingUnsubscribe);
          _callbacks.on_error(client_error::UnsubscribeError);
          _subscriptions.erase(it);
          return;
        }
      }
      ABORT() << "got unexpected unsubscribe error: " << to_string(d);
    } else if (action == "rtm/subscription/error") {
      LOG(ERROR) << "subscription error: " << to_string(d);
      _callbacks.on_error(client_error::SubscriptionError);
    } else {
      ABORT() << "unhandled action " << action << to_string(d);
    }
  }

  enum class client_state { Stopped = 1, Running = 2, PendingStopped = 3 };
  std::atomic<client_state> _client_state{client_state::Stopped};

  const std::string _host;
  const std::string _port;
  const std::string _appkey;
  const uint64_t _client_id;
  error_callbacks &_callbacks;

  asio::ip::tcp::resolver _tcp_resolver;
  boost::beast::websocket::stream<boost::asio::ssl::stream<boost::asio::ip::tcp::socket> >
      _ws;
  uint64_t _request_id{0};
  boost::beast::multi_buffer _read_buffer{READ_BUFFER_SIZE};
  std::map<std::string, subscription_impl> _subscriptions;
};

}  // namespace

std::unique_ptr<client> new_client(const std::string &endpoint, const std::string &port,
                                   const std::string &appkey,
                                   asio::io_service &io_service,
                                   asio::ssl::context &ssl_ctx, size_t id,
                                   error_callbacks &callbacks) {
  LOG(1) << "Creating RTM client for " << endpoint << ":" << port;
  std::unique_ptr<secure_client> client(
      new secure_client(endpoint, port, appkey, id, callbacks, io_service, ssl_ctx));
  return std::move(client);
}
}  // namespace rtm
