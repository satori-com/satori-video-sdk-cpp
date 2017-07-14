#include <iostream>
#include <boost/program_options.hpp>
#include <librtmvideo/rtmpacket.h>
#include <librtmvideo/rtmvideo.h>

#include "base64.h"
#include "rtmclient.h"
#include "video_source_camera.h"
#include "video_source_file.h"

namespace {

struct publisher : public rtm::error_callbacks {
 public:
  publisher(
      const std::string &rtm_endpoint,
      const std::string &rtm_port,
      const std::string &rtm_appkey,
      const std::string &rtm_channel)
      : _frames_channel(rtm_channel),
        _metadata_channel(rtm_channel + metadata_channel_suffix) {

    _rtm_client = rtm::new_client(
        rtm_endpoint, rtm_port, rtm_appkey, _io_service, _ssl_context, 1, *this);
  }

  void on_error(rtm::error e, const std::string &msg) override {
    std::cerr << "ERROR: " << (int)e << " " << msg << "\n";
  }

  void publish_metadata(const char *codec_name, size_t data_len, const uint8_t *data) {
    std::string encoded;
    if(data_len > 0) {
      encoded = std::move(rtm::video::encode64({data, data + data_len}));
    }

    cbor_item_t *packet = rtm::video::metadata_packet(codec_name, encoded);
    _rtm_client->publish(_metadata_channel, packet, nullptr);
    cbor_decref(&packet);
  }

  void publish_frame(size_t data_len, const uint8_t *data) {
    std::string encoded = std::move(rtm::video::encode64({data, data + data_len}));
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
  }

 private:
  boost::asio::io_service _io_service;
  boost::asio::ssl::context _ssl_context{boost::asio::ssl::context::sslv23};
  std::unique_ptr<rtm::client> _rtm_client;
  std::string _frames_channel;
  std::string _metadata_channel;
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

  rtm::video::initialize_sources_library();
  std::unique_ptr<rtm::video::source> video_source;
  
  if(source_type == "camera") {
    video_source = std::make_unique<rtm::video::camera_source>(vm["dimensions"].as<std::string>());
  } else if(source_type == "file") {
    if(!vm.count("file")) {
      std::cerr << "*** File was not specified\n";
      return -1;
    }
    video_source = std::make_unique<rtm::video::file_source>(vm["file"].as<std::string>(), vm.count("replayed"));
  } else {
    std::cerr << "*** Unsupported input type " << source_type << "\n";
    return -1;
  }

  int err = video_source->init();
  if(err) {
    std::cerr << "*** Error initializing video source, error code " << err << "\n";
    return -1;
  }

  publisher p{
      vm["rtm-endpoint"].as<std::string>(),
      vm["rtm-port"].as<std::string>(),
      vm["rtm-appkey"].as<std::string>(),
      vm["rtm-channel"].as<std::string>()};

  video_source->subscribe(
      [&p](const char *codec_name, size_t data_len, const uint8_t *data) {
        p.publish_metadata(codec_name, data_len, data);
      },
      [&p](size_t data_len, const uint8_t *data) {
        p.publish_frame(data_len, data);
      });

  video_source->start();
}