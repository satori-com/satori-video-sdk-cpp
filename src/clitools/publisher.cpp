#include <boost/program_options.hpp>
#include <iostream>

#include "cli_streams.h"
#include "logging_impl.h"
#include "metrics.h"
#include "rtm_client.h"
#include "streams/asio_streams.h"
#include "video_streams.h"

using namespace satori::video;

namespace {

struct rtm_error_handler : rtm::error_callbacks {
  void on_error(std::error_condition ec) override { LOG(ERROR) << ec.message(); }
};

namespace po = boost::program_options;

cli_streams::cli_options cli_configuration() {
  cli_streams::cli_options result;
  result.enable_file_input = true;
  result.enable_camera_input = true;
  result.enable_url_input = true;
  result.enable_rtm_output = true;
  result.enable_generic_output_options = true;

  return result;
}

po::options_description cli_options() {
  po::options_description cli_generic("Generic options");
  cli_generic.add_options()("help", "produce help message");
  cli_generic.add_options()(
      ",v", po::value<std::string>(),
      "log verbosity level (INFO, WARNING, ERROR, FATAL, OFF, 1-9)");

  po::options_description publisher_options("Publisher options");

  return publisher_options.add(cli_generic).add(metrics_options());
}

struct publisher_configuration : cli_streams::configuration {
  publisher_configuration(int argc, char* argv[])
      : configuration(argc, argv, cli_configuration(), cli_options()) {}
};

}  // namespace

// TODO: handle SIGINT, SIGKILL, etc
int main(int argc, char* argv[]) {
  publisher_configuration config{argc, argv};

  init_logging(argc, argv);

  boost::asio::io_service io_service;
  boost::asio::ssl::context ssl_context{boost::asio::ssl::context::sslv23};
  rtm_error_handler error_handler;

  init_metrics(config.metrics(), io_service);

  std::shared_ptr<rtm::client> rtm_client = config.rtm_client(
      io_service, std::this_thread::get_id(), ssl_context, error_handler);

  std::string rtm_channel = config.rtm_channel();

  if (auto ec = rtm_client->start()) {
    ABORT() << "error starting rtm client: " << ec.message();
  }
  expose_metrics(rtm_client.get());

  streams::publisher<satori::video::encoded_packet> source =
      config.encoded_publisher(io_service, rtm_client, rtm_channel);

  source = std::move(source) >> streams::do_finally([&io_service, &rtm_client]() {
             io_service.post([&rtm_client]() {
               stop_metrics();
               if (auto ec = rtm_client->stop()) {
                 LOG(ERROR) << "error stopping rtm client: " << ec.message();
               } else {
                 LOG(INFO) << "rtm client was stopped";
               }
             });
           });

  source->subscribe(config.encoded_subscriber(rtm_client, io_service, rtm_channel));

  io_service.run();
}
