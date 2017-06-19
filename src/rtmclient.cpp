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

#include "rtmclient.h"

namespace asio = boost::asio;

namespace rtm {

using endpoint_iterator_t = asio::ip::tcp::resolver::iterator;
using endpoint_t = asio::ip::tcp::resolver::endpoint_type;

namespace {

static void fail(boost::system::error_code ec) {
  std::cerr << "\nERROR " << ec.category().name() << ':' << ec.value() << " "
            << ec.message() << "\n";
  std::cerr.flush();
  exit(1);
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
  const bool fast_forward{true};

  void serialize_to(rapidjson::Document &document) const {
    constexpr const char *tmpl =
        R"({"action":"rtm/subscribe", "body":{"channel":"test_channel"}, "id": 2})";
    document.Parse(tmpl);
    BOOST_VERIFY(document.IsObject());
    auto body = document["body"].GetObject();
    body["channel"].SetString(channel.c_str(), channel.length(),
                              document.GetAllocator());
  }
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

  virtual ~secure_client() = default;

  void publish(const std::string &channel, const rapidjson::Document &message,
               publish_callbacks *callbacks) {
    BOOST_VERIFY_MSG(false, "NOT IMPLEMENTED");
  }

  void subscribe_channel(const std::string &channel, subscription &sub,
                         subscription_callbacks &callbacks,
                         const subscription_options *options) {
    _subscription_callbacks = &callbacks;
    rapidjson::Document document;
    subscribe_request req{++_request_id, channel};
    req.serialize_to(document);
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    document.Accept(writer);
    _ws.write(asio::buffer(buf.GetString(), buf.GetSize()));
  }

  void subscribe_filter(const std::string &filter, subscription &sub,
                        subscription_callbacks &callbacks,
                        const subscription_options *options) {
    BOOST_VERIFY_MSG(false, "NOT IMPLEMENTED");
  }

  void unsubscribe(subscription &sub) {
    BOOST_VERIFY_MSG(false, "NOT IMPLEMENTED");
  }

  const channel_position &position(subscription &sub) {
    BOOST_VERIFY_MSG(false, "NOT IMPLEMENTED");
  }

  bool is_up(subscription &sub) { BOOST_VERIFY_MSG(false, "NOT IMPLEMENTED"); }

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
      BOOST_VERIFY(subscription_id == "1");

      for (const auto &m : body["messages"].GetArray()) {
        if (_subscription_callbacks) {
          (*_subscription_callbacks).on_data(m);
        }
      }
    }
  }

  uint64_t _client_id;
  error_callbacks &_callbacks;

  beast::websocket::stream<
      boost::asio::ssl::stream<boost::asio::ip::tcp::socket> >
      _ws;
  uint64_t _request_id{0};
  beast::multi_buffer _read_buffer;
  subscription_callbacks *_subscription_callbacks{nullptr};
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
}
