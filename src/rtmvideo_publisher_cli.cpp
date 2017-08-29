#include <boost/program_options.hpp>
#include <iostream>
#include "asio_streams.h"
#include "rtmclient.h"
#include "sink_rtm.h"
#include "video_streams.h"

namespace {

struct rtm_error_handler : public rtm::error_callbacks {
  void on_error(std::error_condition ec) override {
    std::cerr << "ERROR: " << ec.message() << "\n";
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
      "loop", "Is file looped");

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
    std::cerr << "*** --source-type type either was not specified or has invalid value\n";
    return -1;
  }
  if (!vm.count("endpoint")) {
    std::cerr << "*** --endpoint was not specified\n";
    return -1;
  }
  if (!vm.count("appkey")) {
    std::cerr << "*** --appkey was not specified\n";
    return -1;
  }
  if (!vm.count("channel")) {
    std::cerr << "*** --channel was not specified\n";
    return -1;
  }

  boost::asio::io_service io_service;

  rtm::video::initialize_source_library();
  streams::publisher<rtm::video::encoded_packet> source;

  if (source_type == "camera") {
    source = rtm::video::camera_source(io_service, vm["dimensions"].as<std::string>());
  } else if (source_type == "file") {
    if (!vm.count("file")) {
      std::cerr << "*** File was not specified\n";
      return -1;
    }
    source = rtm::video::file_source(io_service, vm["file"].as<std::string>(),
                                     vm.count("loop"), true);
  } else {
    std::cerr << "*** Unsupported input type " << source_type << "\n";
    return -1;
  }

  boost::asio::ssl::context ssl_context{boost::asio::ssl::context::sslv23};
  rtm_error_handler error_handler;
  std::shared_ptr<rtm::publisher> rtm_client = rtm::new_client(
      vm["endpoint"].as<std::string>(), vm["port"].as<std::string>(),
      vm["appkey"].as<std::string>(), io_service, ssl_context, 1,
      error_handler);

  source->subscribe(
      *(new rtm::video::rtm_sink(rtm_client, vm["channel"].as<std::string>())));

  io_service.run();
}
