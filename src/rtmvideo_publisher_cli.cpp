#include <iostream>
#include <boost/bind.hpp>
#include <boost/program_options.hpp>
#include <librtmvideo/rtmpacket.h>
#include <librtmvideo/rtmvideo.h>
#include <librtmvideo/video_source.h>

#include "base64.h"
#include "rtmclient.h"

namespace {

struct rtm_errors_handler : public rtm::error_callbacks {
  void on_error(rtm::error e, const std::string &msg) override {
    std::cerr << "ERROR: " << (int)e << " " << msg << "\n";
  }
};

struct publisher {
 public:
  publisher(
      video_source *video_source,
      boost::asio::io_service &io_service,
      std::unique_ptr<rtm::client> rtm_client,
      const std::string &rtm_channel)
      : _video_source(video_source),
        _io_service(io_service),
        _rtm_client(std::move(rtm_client)),
        _frames_channel(rtm_channel),
        _metadata_channel(rtm_channel + metadata_channel_suffix),
        _frames_interval(boost::posix_time::milliseconds(1000.0 / video_source_fps(_video_source))),
        _metadata_interval(boost::posix_time::seconds(10)),
        _frames_timer(io_service),
        _metadata_timer(io_service)
  {}

  void start() {
    _frames_timer.expires_from_now(_frames_interval);
    _frames_timer.async_wait(boost::bind(&publisher::frames_tick, this));

    _metadata_timer.expires_from_now(boost::posix_time::seconds(0));
    _metadata_timer.async_wait(boost::bind(&publisher::metadata_tick, this));

    _io_service.run();
  }

 private:
  void frames_tick() {
    uint8_t *raw_data = nullptr;
    int raw_data_size = video_source_next_packet(_video_source, &raw_data);
    if(raw_data_size <= 0) {
      _io_service.stop();
      return;
    }

    std::string encoded = std::move(rtm::video::encode64({raw_data, raw_data + raw_data_size}));
    delete[] raw_data;

    size_t nb_chunks = std::ceil((double) encoded.length() / rtm::video::max_payload_size);

    for(size_t i = 0; i < nb_chunks; i++) {
      cbor_item_t *packet = rtm::video::frame_packet(
          encoded.substr(i * rtm::video::max_payload_size, rtm::video::max_payload_size),
          _seq_id,
          _seq_id,
          std::chrono::system_clock::now(),
          i + 1,
          nb_chunks);
      _rtm_client->publish(_frames_channel, packet, nullptr);
      cbor_decref(&packet);
    }

    _seq_id++;
    if(_seq_id % 100 == 0) {
      std::cout << "Published " << _seq_id << " frames\n";
    }

    _frames_timer.expires_at(_frames_timer.expires_at() + _frames_interval);
    _frames_timer.async_wait(boost::bind(&publisher::frames_tick, this));
  }

  void metadata_tick() {
    std::string codec_name(video_source_codec_name(_video_source));
    uint8_t *raw_data = nullptr;
    int raw_data_size = video_source_codec_data(_video_source, &raw_data);

    std::string encoded;
    if(raw_data_size > 0) {
      encoded = std::move(rtm::video::encode64({raw_data, raw_data + raw_data_size}));
      delete[] raw_data;
    }

    cbor_item_t *packet = rtm::video::metadata_packet(codec_name, encoded);
    _rtm_client->publish(_metadata_channel, packet, nullptr);
    cbor_decref(&packet);

    _metadata_timer.expires_at(_metadata_timer.expires_at() + _metadata_interval);
    _metadata_timer.async_wait(boost::bind(&publisher::metadata_tick, this));
  }

 private:
  video_source *_video_source{nullptr};
  boost::asio::io_service &_io_service;
  std::unique_ptr<rtm::client> _rtm_client;
  std::string _frames_channel;
  std::string _metadata_channel;
  boost::posix_time::time_duration _frames_interval;
  boost::posix_time::time_duration _metadata_interval;
  boost::asio::deadline_timer _frames_timer;
  boost::asio::deadline_timer _metadata_timer;
  uint64_t _seq_id{0};
};
}

// TODO: handle SIGINT, SIGKILL, etc
int main(int argc, char *argv[]) {
  std::string source_type;

  namespace po = boost::program_options;
  po::options_description generic_options("Generic options");
  generic_options.add_options()
      ("help,h", "produce help message")
      ("source-type,t", po::value<std::string>(&source_type), "Source type: [file|camera]");

  po::options_description rtm_options("RTM connection options");
  rtm_options.add_options()
      ("rtm-endpoint,e", po::value<std::string>(), "RTM endpoint")
      ("rtm-appkey,k", po::value<std::string>(), "RTM appkey")
      ("rtm-channel,c", po::value<std::string>(), "RTM channel")
      ("rtm-port,p", po::value<std::string>()->default_value("443"), "RTM port");

  po::options_description file_options("File options");
  file_options.add_options()
      ("file,f", po::value<std::string>(), "Source file")
      ("replayed,r", po::value<std::string>()->implicit_value(""), "Is file replayed");

  po::options_description camera_options("Camera options");
  camera_options.add_options()
      ("dimensions,d", po::value<std::string>()->default_value("320x240"), "Dimensions");

  po::options_description cmdline_options;
  cmdline_options
      .add(generic_options)
      .add(rtm_options)
      .add(file_options)
      .add(camera_options);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
  po::notify(vm);

  if(vm.count("help") || argc == 1) {
    std::cout << cmdline_options << "\n";
    return 1;
  }
  if(!vm.count("source-type") || (source_type != "camera" && source_type != "file")) {
    std::cerr << "*** Source type either was not specified or has invalid value\n";
    return -1;
  }
  if(!vm.count("rtm-endpoint")) {
    std::cerr << "*** RTM endpoint was not specified\n";
    return -1;
  }
  if(!vm.count("rtm-appkey")) {
    std::cerr << "*** RTM appkey was not specified\n";
    return -1;
  }
  if(!vm.count("rtm-channel")) {
    std::cerr << "*** RTM channel was not specified\n";
    return -1;
  }

  video_source *video_source;
  video_source_init_library();
  if(source_type == "camera") {
    video_source = video_source_camera_new(vm["dimensions"].as<std::string>().c_str());
  } else if(source_type == "file") {
    if(!vm.count("file")) {
      std::cerr << "*** File was not specified\n";
      return -1;
    }
    video_source = video_source_file_new(vm["file"].as<std::string>().c_str(), vm.count("replayed"));
  } else {
    std::cerr << "*** Unsupported input type " << source_type << "\n";
    return -1;
  }

  if(!video_source) {
    std::cerr << "*** Failed to initialize video source\n";
    return -1;
  }

  boost::asio::io_service io_service;
  boost::asio::ssl::context ssl_context{boost::asio::ssl::context::sslv23};
  rtm_errors_handler rtm_errors_handler;
  std::unique_ptr<rtm::client> rtm_client{rtm::new_client(
      vm["rtm-endpoint"].as<std::string>(),
      vm["rtm-port"].as<std::string>(),
      vm["rtm-appkey"].as<std::string>(),
      io_service, ssl_context, 1, rtm_errors_handler)};

  publisher publisher{video_source, io_service, std::move(rtm_client), vm["rtm-channel"].as<std::string>()};

  publisher.start();
}