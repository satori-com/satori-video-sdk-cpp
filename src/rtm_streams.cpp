#include "rtm_streams.h"

namespace streams {
namespace rtm {

struct rtm_channel_impl : ::rtm::subscription_callbacks {
  rtm_channel_impl(std::shared_ptr<::rtm::subscriber> subscriber,
                   const std::string &channel, const ::rtm::subscription_options &options,
                   streams::observer<cbor_item_t *> &sink)
      : _subscriber(subscriber), _sink(sink) {
    _subscriber->subscribe_channel(channel, _subscription, *this, &options);
  }

  ~rtm_channel_impl() override { _subscriber->unsubscribe(_subscription); }

  void on_data(const ::rtm::subscription &sub, cbor_item_t *data) override {
    _sink.on_next(std::move(data));
  }

  const std::shared_ptr<::rtm::subscriber> _subscriber;
  streams::observer<cbor_item_t *> &_sink;
  ::rtm::subscription _subscription;
};

struct cbor_sink_impl : public streams::subscriber<cbor_item_t *> {
  cbor_sink_impl(std::shared_ptr<::rtm::publisher> client, const std::string &channel)
      : _client(client), _channel(channel) {}

  void on_next(cbor_item_t *&&item) override {
    _client->publish(_channel, item, nullptr);
    cbor_decref(&item);
    if (_src) {
      _src->request(1);
    }
  }

  void on_error(std::error_condition ec) override { ABORT() << ec.message(); }

  void on_complete() override { delete this; }

  void on_subscribe(streams::subscription &s) override {
    _src = &s;
    _src->request(1);
  }

  const std::shared_ptr<::rtm::publisher> _client;
  const std::string _channel;
  streams::subscription *_src{nullptr};
};

streams::subscriber<cbor_item_t *> &cbor_sink(std::shared_ptr<::rtm::publisher> client,
                                              const std::string &channel) {
  return *(new cbor_sink_impl(client, channel));
}

streams::publisher<cbor_item_t *> cbor_channel(
    std::shared_ptr<::rtm::subscriber> subscriber, const std::string &channel,
    const ::rtm::subscription_options &options) {
  return streams::generators<cbor_item_t *>::async<rtm_channel_impl>(
      [subscriber, channel, options](streams::observer<cbor_item_t *> &observer) {
        return new rtm_channel_impl(subscriber, channel, options, observer);
      },
      [](rtm_channel_impl *impl) { delete impl; });
  ;
}

}  // namespace rtm
}  // namespace streams