#include <assert.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <beast/core.hpp>
#include <beast/websocket.hpp>
#include <beast/websocket/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <deque>
#include <iostream>
#include <memory>
#include <utility>

#include "cbor_json.h"
#include "rtmclient.h"

namespace asio = boost::asio;

namespace rtm {

using endpoint_iterator_t = asio::ip::tcp::resolver::iterator;
using endpoint_t = asio::ip::tcp::resolver::endpoint_type;

namespace {

constexpr int READ_BUFFER_SIZE = 100000;

static void fail(boost::system::error_code ec) {
  std::cerr << "\nERROR " << ec.category().name() << ':' << ec.value() << " "
            << ec.message() << "\n";
  std::cerr.flush();
  exit(1);
}

static rapidjson::Value cbor_to_json(const cbor_item_t *item,
                                     rapidjson::Document &document) {
  rapidjson::Value a;
  switch (cbor_typeof(item)) {
    case CBOR_TYPE_NEGINT:
    case CBOR_TYPE_UINT:
      a = rapidjson::Value(cbor_get_int(item));
      break;
    case CBOR_TYPE_TAG:
    case CBOR_TYPE_BYTESTRING:
      BOOST_VERIFY_MSG(false, "NOT IMPLEMENTED");
      break;
    case CBOR_TYPE_STRING:
      if (cbor_string_is_indefinite(item)) {
        BOOST_VERIFY_MSG(false, "NOT IMPLEMENTED");
      } else {
        // unsigned char * -> char *
        a.SetString(reinterpret_cast<char *>(cbor_string_handle(item)),
                    static_cast<int>(cbor_string_length(item)),
                    document.GetAllocator());
      }
      break;
    case CBOR_TYPE_ARRAY:
      a = rapidjson::Value(rapidjson::kArrayType);
      for (size_t i = 0; i < cbor_array_size(item); i++)
        a.PushBack(cbor_to_json(cbor_array_handle(item)[i], document),
                   document.GetAllocator());
      break;
    case CBOR_TYPE_MAP:
      a = rapidjson::Value(rapidjson::kObjectType);
      for (size_t i = 0; i < cbor_map_size(item); i++)
        a.AddMember(cbor_to_json(cbor_map_handle(item)[i].key, document),
                    cbor_to_json(cbor_map_handle(item)[i].value, document),
                    document.GetAllocator());
      break;
    case CBOR_TYPE_FLOAT_CTRL:
      a = rapidjson::Value(cbor_float_get_float(item));
      break;
  }
  return a;
}

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
    BOOST_VERIFY(document.IsObject());
    document["id"].SetInt64(id);
    auto body = document["body"].GetObject();
    body["channel"].SetString(channel.c_str(), channel.length(),
                              document.GetAllocator());
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

struct subscription_impl {
  const subscription &sub;
  subscription_callbacks &callbacks;
};

class secure_client : public client {
 public:
  explicit secure_client(std::string host, std::string port, std::string appkey,
                         uint64_t client_id, error_callbacks &callbacks,
                         asio::io_service &io_service,
                         asio::ssl::context &ssl_ctx)
      : _ws{io_service, ssl_ctx}, _client_id(client_id), _callbacks(callbacks) {
    boost::system::error_code ec;

    asio::ip::tcp::resolver tcp_resolver(io_service);
    auto endpoints = tcp_resolver.resolve({host, port}, ec);
    if (ec) fail(ec);

    _ws.read_message_max(READ_BUFFER_SIZE);

    // tcp connect
    asio::connect(_ws.next_layer().next_layer(), endpoints, ec);
    if (ec) fail(ec);

    // ssl handshake
    _ws.next_layer().handshake(boost::asio::ssl::stream_base::client);

    // upgrade to ws.
    _ws.handshake(host, "/v2?appkey=" + appkey, ec);
    if (ec) fail(ec);
    std::cout << "Websocket open\n";
    ask_for_read();
  }

  ~secure_client() override = default;

  void publish(const std::string &channel, const cbor_item_t *message,
               publish_callbacks *callbacks) override {
    BOOST_VERIFY_MSG(callbacks == nullptr, "NOT IMPLEMENTED");
    rapidjson::Document document;
    constexpr const char *tmpl =
        R"({"action":"rtm/publish",
            "body":{"channel":"<not_set>"}})";
    document.Parse(tmpl);
    BOOST_VERIFY(document.IsObject());
    auto body = document["body"].GetObject();
    body["channel"].SetString(channel.c_str(), channel.length(),
                              document.GetAllocator());

    body.AddMember("message", cbor_to_json(message, document),
                   document.GetAllocator());

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    document.Accept(writer);
    _ws.write(asio::buffer(buf.GetString(), buf.GetSize()));
  }

  void subscribe_channel(const std::string &channel, const subscription &sub,
                         subscription_callbacks &callbacks,
                         const subscription_options *options) override {
    _subscriptions.emplace(
        std::make_pair(channel, subscription_impl{sub, callbacks}));

    rapidjson::Document document;
    subscribe_request req{++_request_id, channel};
    if (options) {
      req.age = options->history.age;
      req.count = options->history.count;
    }
    req.serialize_to(document);
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    document.Accept(writer);
    _ws.write(asio::buffer(buf.GetString(), buf.GetSize()));
  }

  void subscribe_filter(const std::string &filter, const subscription &sub,
                        subscription_callbacks &callbacks,
                        const subscription_options *options) override {
    BOOST_VERIFY_MSG(false, "NOT IMPLEMENTED");
  }

  void unsubscribe(const subscription &sub) override {
    BOOST_VERIFY_MSG(false, "NOT IMPLEMENTED");
  }

  const channel_position &position(const subscription &sub) override {
    BOOST_VERIFY_MSG(false, "NOT IMPLEMENTED");
  }

  bool is_up(const subscription &sub) override {
    BOOST_VERIFY_MSG(false, "NOT IMPLEMENTED");
  }

 private:
  void ask_for_read() {
    _ws.async_read(_read_buffer, [this](boost::system::error_code const &ec) {
      if (ec) fail(ec);

      std::string input =
          boost::lexical_cast<std::string>(buffers(_read_buffer.data()));
      _read_buffer.consume(_read_buffer.size());

      rapidjson::StringStream s(input.c_str());
      rapidjson::Document d;
      d.ParseStream(s);
      process_input(d);

      ask_for_read();
    });
  }

  void process_input(const rapidjson::Document &d) {
    if (!d.HasMember("action")) {
      std::cerr << "no action in pdu: " << to_string(d) << "\n";
    }

    std::string action = d["action"].GetString();
    if (action == "rtm/subscription/data") {
      auto body = d["body"].GetObject();
      std::string subscription_id = body["subscription_id"].GetString();
      auto it = _subscriptions.find(subscription_id);
      BOOST_VERIFY(it != _subscriptions.end());
      subscription_impl &sub = it->second;
      for (const auto &m : body["messages"].GetArray()) {
        sub.callbacks.on_data(sub.sub, m);
      }
    } else if (action == "rtm/subscribe/ok") {
      // ignore
    } else if (action == "rtm/subscription/error") {
      _callbacks.on_error(error::SubscriptionError, to_string(d));
    } else {
      std::cerr << "unhandled action " << action << "\n"
                << to_string(d) << "\n";
      BOOST_VERIFY(false);
    }
  }

  uint64_t _client_id;
  error_callbacks &_callbacks;

  beast::websocket::stream<
      boost::asio::ssl::stream<boost::asio::ip::tcp::socket> >
      _ws;
  uint64_t _request_id{0};
  beast::multi_buffer _read_buffer{READ_BUFFER_SIZE};
  std::map<std::string, subscription_impl> _subscriptions;
};

}  // namespace

std::unique_ptr<client> new_client(const std::string &endpoint,
                                   const std::string &port,
                                   const std::string &appkey,
                                   asio::io_service &io_service,
                                   asio::ssl::context &ssl_ctx, size_t id,
                                   error_callbacks &callbacks) {
  std::cout << "Creating RTM client for " << endpoint << ":" << port << "\n";
  std::unique_ptr<secure_client> client(new secure_client(
      endpoint, port, appkey, id, callbacks, io_service, ssl_ctx));
  return std::move(client);
}
}  // namespace rtm
