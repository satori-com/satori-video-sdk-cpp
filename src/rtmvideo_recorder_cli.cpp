#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/program_options.hpp>
#include <memory>
#include <string>

#include "librtmvideo/data.h"
#include "logging_implementation.h"
#include "rtmclient.h"
#include "video_streams.h"
#include "vp9_encoder.h"
#include "worker.h"

namespace {
constexpr size_t network_frames_max_buffer_size = 1024;
constexpr size_t incoming_encoded_frames_max_buffer_size = 1024;
constexpr size_t outgoing_encoded_frames_max_buffer_size = 1024;
constexpr size_t image_frames_max_buffer_size = 1024;

struct rtm_error_handler : public rtm::error_callbacks {
  void on_error(std::error_condition ec) override { LOG_S(ERROR) << ec.message(); }
};

struct interruption_handler {
  void interrupt() {
    LOG_S(INFO) << "Received STOP request";
    running = false;
  }

  std::atomic<bool> running{true};
};
}  // namespace

int main(int argc, char* argv[]) {
  init_logging(argc, argv);
  namespace po = boost::program_options;

  // TODO: should be some common module cliutils.h
  po::options_description generic_options("Generic options");
  generic_options.add_options()("help", "produce help message");

  po::options_description rtm_options("RTM connection options");
  rtm_options.add_options()("endpoint", po::value<std::string>(), "RTM endpoint")(
      "appkey", po::value<std::string>(), "RTM appkey")(
      "channel", po::value<std::string>(), "RTM channel")(
      "port", po::value<std::string>()->default_value("443"), "RTM port");

  po::options_description input_file_options("input file options");
  input_file_options.add_options()("input-file", po::value<std::string>(),
                                   "Input video file");

  po::options_description output_file_options("output file options");
  output_file_options.add_options()("output-file", po::value<std::string>(),
                                    "Output video file");

  po::options_description cmdline_options;
  cmdline_options.add(generic_options)
      .add(rtm_options)
      .add(input_file_options)
      .add(output_file_options);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
  po::notify(vm);

  if (vm.count("help") || argc == 1) {
    std::cout << cmdline_options << "\n";
    return 1;
  }

  const bool is_rtm_source =
      vm.count("endpoint") || vm.count("appkey") || vm.count("channel");
  const bool is_file_source = vm.count("input-file");
  if (is_rtm_source && is_file_source) {
    std::cerr << "Choose either RTM or video file as an input\n";
    return 1;
  }

  if (is_rtm_source) {
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
  }
  if (!vm.count("output-file")) {
    std::cerr << "*** output file was not specified\n";
    return -1;
  }

  streams::publisher<rtm::video::encoded_packet> incoming_packets_source;
  boost::asio::io_service io_service;
  std::shared_ptr<rtm::client> rtm_client{nullptr};
  ::interruption_handler ih;

  boost::asio::signal_set signals(io_service);
  signals.add(SIGINT);
  signals.add(SIGTERM);
  signals.add(SIGQUIT);
  signals.async_wait(std::bind(&::interruption_handler::interrupt, &ih));

  if (is_rtm_source) {
    boost::asio::ssl::context ssl_context{boost::asio::ssl::context::sslv23};
    rtm_error_handler error_handler;
    rtm_client = rtm::new_client(
        vm["endpoint"].as<std::string>(), vm["port"].as<std::string>(),
        vm["appkey"].as<std::string>(), io_service, ssl_context, 1, error_handler);
    rtm_client->start();

    incoming_packets_source =
        rtm::video::rtm_source(rtm_client, vm["channel"].as<std::string>())
        >> rtm::video::buffered_worker("recorder.network_buffer",
                                       network_frames_max_buffer_size)
        >> streams::lift(rtm::video::decode_network_stream())
        >> rtm::video::buffered_worker("recorder.encoded_buffer",
                                       incoming_encoded_frames_max_buffer_size);
  } else {
    incoming_packets_source = rtm::video::file_source(
        io_service, vm["input-file"].as<std::string>(), false, false);
  }

  streams::publisher<rtm::video::encoded_packet> outgoing_packets_source =
      std::move(incoming_packets_source) >> streams::lift(rtm::video::decode_image_frames(
                                                640, 480, image_pixel_format::RGB0))
      >> streams::take_while(
             [&ih](const rtm::video::owned_image_packet&) { return ih.running.load(); })
      >> rtm::video::buffered_worker("recorder.image_buffer",
                                     image_frames_max_buffer_size)
      >> streams::lift(rtm::video::encode_vp9(25))
      >> rtm::video::buffered_worker("recorder.vp9_encoded_buffer",
                                     outgoing_encoded_frames_max_buffer_size)
      >> streams::do_finally([rtm_client, &io_service]() {
          LOG_S(INFO) << "Stopping RTM client";
          rtm_client->stop();
          io_service.stop();  // TODO: not the best thing
        });

  LOG_S(INFO) << "Starting recording...";

  streams::subscriber<rtm::video::encoded_packet>& sink =
      rtm::video::mkv_sink(vm["output-file"].as<std::string>());
  outgoing_packets_source->subscribe(sink);

  io_service.run();

  LOG_S(INFO) << "Recording is done";
}