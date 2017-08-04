#include <librtmvideo/rtmpacket.h>
#include <librtmvideo/rtmvideo.h>
#include <boost/program_options.hpp>
#include <iostream>
#include "rtmclient.h"
#include "sink_rtm.h"
#include "source_camera.h"
#include "source_file.h"

namespace {

struct rtm_error_handler : public rtm::error_callbacks {
  void on_error(rtm::error e, const std::string &msg) override {
    std::cerr << "ERROR: " << (int)e << " " << msg << "\n";
  }
};

}  // namespace

// TODO: handle SIGINT, SIGKILL, etc
int main(int argc, char *argv[]) {
  std::string source_type;

  namespace po = boost::program_options;
  po::options_description generic_options("Generic options");
  generic_options.add_options()("help", "produce help message")(
      "source-type", po::value<std::string>(&source_type),
      "Source type: [file|camera]");

  po::options_description rtm_options("RTM connection options");
  rtm_options.add_options()("endpoint", po::value<std::string>(),
                            "RTM endpoint")("appkey", po::value<std::string>(),
                                            "RTM appkey")(
      "channel", po::value<std::string>(), "RTM channel")(
      "port", po::value<std::string>()->default_value("443"), "RTM port");

  po::options_description file_options("File options");
  file_options.add_options()("file", po::value<std::string>(), "Source file")(
      "replayed", "Is file replayed");

  po::options_description camera_options("Camera options");
  camera_options.add_options()(
      "dimensions", po::value<std::string>()->default_value("320x240"),
      "Dimensions");

  po::options_description cmdline_options;
  cmdline_options.add(generic_options)
      .add(rtm_options)
      .add(file_options)
      .add(camera_options);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
  po::notify(vm);

  if (vm.count("help") || argc == 1) {
    std::cout << cmdline_options << "\n";
    return 1;
  }
  if (!vm.count("source-type") ||
      (source_type != "camera" && source_type != "file")) {
    std::cerr
        << "*** Source type either was not specified or has invalid value\n";
    return -1;
  }
  if (!vm.count("endpoint")) {
    std::cerr << "*** RTM endpoint was not specified\n";
    return -1;
  }
  if (!vm.count("appkey")) {
    std::cerr << "*** RTM appkey was not specified\n";
    return -1;
  }
  if (!vm.count("channel")) {
    std::cerr << "*** RTM channel was not specified\n";
    return -1;
  }

  rtm::video::initialize_sources_library();
  std::unique_ptr<rtm::video::source> source;

  if (source_type == "camera") {
    source = std::make_unique<rtm::video::camera_source>(
        vm["dimensions"].as<std::string>());
  } else if (source_type == "file") {
    if (!vm.count("file")) {
      std::cerr << "*** File was not specified\n";
      return -1;
    }
    source = std::make_unique<rtm::video::file_source>(
        vm["file"].as<std::string>(), vm.count("replayed"), false);
  } else {
    std::cerr << "*** Unsupported input type " << source_type << "\n";
    return -1;
  }

  int err = source->init();
  if (err) {
    std::cerr << "*** Error initializing video source, error code " << err
              << "\n";
    return -1;
  }

  boost::asio::io_service io_service;
  boost::asio::ssl::context ssl_context{boost::asio::ssl::context::sslv23};
  rtm_error_handler error_handler;
  std::shared_ptr<rtm::publisher> rtm_client = rtm::new_client(
      vm["endpoint"].as<std::string>(), vm["port"].as<std::string>(),
      vm["appkey"].as<std::string>(), io_service, ssl_context, 1,
      error_handler);

  auto sink = std::make_unique<rtm::video::rtm_sink>(
      rtm_client, vm["channel"].as<std::string>());
  source->subscribe(std::move(sink));
  source->start();
}
