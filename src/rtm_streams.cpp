#include "rtm_streams.h"

namespace satori {
namespace video {
namespace rtm {

namespace {

class rtm_channel_impl : rtm::subscription_callbacks {
 public:
  rtm_channel_impl(const std::shared_ptr<rtm::subscriber> &subscriber,
                   const std::string &channel, const rtm::subscription_options &options,
                   streams::observer<channel_data> &sink)
      : _subscriber(subscriber), _sink(sink) {
    _subscriber->subscribe_channel(channel, _subscription, *this, &options);
  }

  ~rtm_channel_impl() override { _subscriber->unsubscribe(_subscription); }

 private:
  void on_data(const rtm::subscription & /*sub*/, channel_data &&data) override {
    _sink.on_next(std::move(data));
  }

  void on_error(std::error_condition ec) override { _sink.on_error(ec); }

  const std::shared_ptr<rtm::subscriber> _subscriber;
  streams::observer<channel_data> &_sink;
  rtm::subscription _subscription;
};

class sink_impl : public streams::subscriber<nlohmann::json> {
 public:
  sink_impl(const std::shared_ptr<rtm::publisher> &client,
            boost::asio::io_service &io_service, const std::string &channel)
      : _client(client), _io_service(io_service), _channel(channel) {}

 private:
  void on_next(nlohmann::json &&item) override {
    _io_service.post([
      client = _client, channel = _channel, item = std::move(item)
    ]() mutable { client->publish(channel, std::move(item), nullptr); });

    if (_src != nullptr) {
      _src->request(1);
    }
  }

  void on_error(std::error_condition ec) override { ABORT() << ec.message(); }

  void on_complete() override { delete this; }

  void on_subscribe(streams::subscription &s) override {
    _src = &s;
    _src->request(1);
  }

  const std::shared_ptr<rtm::publisher> _client;
  boost::asio::io_service &_io_service;
  const std::string _channel;
  streams::subscription *_src{nullptr};
};

}  // namespace

streams::subscriber<nlohmann::json> &sink(const std::shared_ptr<rtm::publisher> &client,
                                          boost::asio::io_service &io_service,
                                          const std::string &channel) {
  return *(new sink_impl(client, io_service, channel));
}

streams::publisher<channel_data> channel(
    const std::shared_ptr<rtm::subscriber> &subscriber, const std::string &channel,
    const rtm::subscription_options &options) {
  return streams::generators<channel_data>::async<rtm_channel_impl>(
             [subscriber, channel, options](streams::observer<channel_data> &observer) {
               return new rtm_channel_impl(subscriber, channel, options, observer);
             },
             [](rtm_channel_impl *impl) { delete impl; })
         >> streams::flatten();
}

}  // namespace rtm
}  // namespace video
}  // namespace satori
