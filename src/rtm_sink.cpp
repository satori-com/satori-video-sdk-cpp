#include "video_streams.h"

#include <chrono>
#include <iostream>
#include <thread>

#include "data.h"
#include "metrics.h"
#include "satori_video.h"
#include "streams/streams.h"

namespace satori {
namespace video {

namespace {

auto &frame_publish_delay_milliseconds =
    prometheus::BuildHistogram()
        .Name("frame_publish_delay_milliseconds")
        .Register(metrics_registry())
        .Add({}, std::vector<double>{0,    0.1,  0.2,  0.3,  0.4,  0.5,  0.6,  0.7,  0.8,
                                     0.9,  1,    2,    3,    4,    5,    6,    7,    8,
                                     9,    10,   15,   20,   25,   30,   40,   50,   60,
                                     70,   80,   90,   100,  200,  300,  400,  500,  600,
                                     700,  800,  900,  1000, 2000, 3000, 4000, 5000, 6000,
                                     7000, 8000, 9000, 10000});

class rtm_sink_impl : public streams::subscriber<encoded_packet>,
                      rtm::request_callbacks,
                      boost::static_visitor<void> {
 public:
  rtm_sink_impl(const std::shared_ptr<rtm::publisher> &client,
                boost::asio::io_service &io_service, const std::string &rtm_channel)
      : _client{client},
        _io_service{io_service},
        _frames_channel{rtm_channel},
        _metadata_channel{rtm_channel + metadata_channel_suffix} {}

  void operator()(const encoded_metadata &m) {
    nlohmann::json packet = m.to_network().to_json();

    _in_flight++;
    _io_service.post([ this, packet = std::move(packet) ]() mutable {
      _client->publish(_metadata_channel, std::move(packet), this);
    });
  }

  void operator()(const encoded_frame &f) {
    std::vector<network_frame> network_frames = f.to_network();

    for (const network_frame &nf : network_frames) {
      nlohmann::json packet = nf.to_json();

      _in_flight++;
      _io_service.post([
        this, packet = std::move(packet), creation_time = f.creation_time
      ]() mutable {
        frame_publish_delay_milliseconds.Observe(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now() - creation_time)
                .count());
        _client->publish(_frames_channel, std::move(packet), this);
      });
    }

    _frames_counter++;
    if (_frames_counter % 100 == 0) {
      LOG(INFO) << "published " << _frames_counter << " frames to " << _frames_channel;
    }
  }

 private:
  void on_next(encoded_packet &&packet) override {
    boost::apply_visitor(*this, packet);
    _src->request(1);
  }

  void on_error(std::error_condition ec) override { ABORT() << ec.message(); }

  void on_complete() override {
    const std::chrono::seconds time_to_wait{30};
    const std::chrono::system_clock::time_point begin = std::chrono::system_clock::now();
    while (_in_flight > 0
           && std::chrono::duration_cast<std::chrono::seconds>(
                  std::chrono::system_clock::now() - begin)
                  < time_to_wait) {
      LOG(2) << "Waiting for packets to be published: " << _in_flight;
      std::this_thread::sleep_for(std::chrono::seconds{1});
    }
    if (_in_flight > 0) {
      LOG(ERROR) << "Not all packets were published: " << _in_flight;
    } else {
      LOG(INFO) << "Packets were published";
    }
    delete this;
  }

  void on_subscribe(streams::subscription &s) override {
    _src = &s;
    _src->request(1);
  }

  void on_ok() override { _in_flight--; }

  const std::shared_ptr<rtm::publisher> _client;
  boost::asio::io_service &_io_service;
  const std::string _frames_channel;
  const std::string _metadata_channel;
  streams::subscription *_src;
  uint64_t _frames_counter{0};
  std::atomic_uint32_t _in_flight{0};
};
}  // namespace

streams::subscriber<encoded_packet> &rtm_sink(
    const std::shared_ptr<rtm::publisher> &client, boost::asio::io_service &io_service,
    const std::string &rtm_channel) {
  return *(new rtm_sink_impl(client, io_service, rtm_channel));
}

}  // namespace video
}  // namespace satori
