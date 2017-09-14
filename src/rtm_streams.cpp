#include "rtm_streams.h"

#include "cbor_json.h"

namespace streams {
namespace rtm {

struct rtm_channel_impl : ::rtm::subscription_callbacks {
  rtm_channel_impl(std::shared_ptr<::rtm::subscriber> subscriber,
                   const std::string &channel, const ::rtm::subscription_options &options,
                   streams::observer<rapidjson::Value> &sink)
      : _subscriber(subscriber), _sink(sink) {
    _subscriber->subscribe_channel(channel, _subscription, *this, &options);
  }

  ~rtm_channel_impl() override { _subscriber->unsubscribe(_subscription); }

  void on_data(const ::rtm::subscription &sub, rapidjson::Value &&value) override {
    _sink.on_next(std::move(value));
  }

  const std::shared_ptr<::rtm::subscriber> _subscriber;
  streams::observer<rapidjson::Value> &_sink;
  ::rtm::subscription _subscription;
};

streams::publisher<rapidjson::Value> json_channel(
    std::shared_ptr<::rtm::subscriber> subscriber, const std::string &channel,
    const ::rtm::subscription_options &options) {
  return streams::generators<rapidjson::Value>::async<rtm_channel_impl>(
      [subscriber, channel, options](streams::observer<rapidjson::Value> &observer) {
        return new rtm_channel_impl(subscriber, channel, options, observer);
      },
      [](rtm_channel_impl *impl) { delete impl; });
}

struct cbor_sink_impl : public streams::subscriber<cbor_item_t *> {
  cbor_sink_impl(std::shared_ptr<::rtm::publisher> client, const std::string &channel)
      : _client(client), _channel(channel) {}

  void on_next(cbor_item_t *&&item) override {
    _client->publish(_channel, item, nullptr);
    cbor_decref(&item);
    _src->request(1);
  }

  void on_error(std::error_condition ec) override {
    std::cerr << "ERROR: " << ec.message() << "\n";
    exit(1);
  }

  void on_complete() override { delete this; }

  void on_subscribe(streams::subscription &s) override {
    _src = &s;
    _src->request(1);
  }

  const std::shared_ptr<::rtm::publisher> _client;
  const std::string _channel;
  streams::subscription *_src;
};

streams::subscriber<cbor_item_t *> &cbor_sink(std::shared_ptr<::rtm::publisher> client,
                                              const std::string &channel) {
  return *(new cbor_sink_impl(client, channel));
}

streams::publisher<cbor_item_t *> cbor_channel(
    std::shared_ptr<::rtm::subscriber> subscriber, const std::string &channel,
    const ::rtm::subscription_options &options) {
  return json_channel(subscriber, channel, options)
         >> streams::map([](rapidjson::Value &&v) { return json_to_cbor(v); });
}

}  // namespace rtm
}  // namespace streams