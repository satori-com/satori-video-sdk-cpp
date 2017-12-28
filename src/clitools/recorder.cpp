#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/program_options.hpp>
#include <memory>
#include <string>

#include "cli_streams.h"
#include "data.h"
#include "logging_impl.h"
#include "rtm_client.h"
#include "streams/signal_breaker.h"
#include "streams/threaded_worker.h"
#include "video_streams.h"
#include "vp9_encoder.h"

using namespace satori::video;

namespace {
constexpr size_t image_buffer_size = 1024;
constexpr size_t outgoing_encoded_frames_max_buffer_size = 1024;

struct rtm_error_handler : rtm::error_callbacks {
  void on_error(std::error_condition ec) override { LOG(ERROR) << ec.message(); }
};

namespace po = boost::program_options;

cli_streams::cli_options cli_configuration() {
  cli_streams::cli_options result;
  result.enable_file_output = true;
  result.enable_camera_input = true;
  result.enable_url_input = true;
  result.enable_rtm_input = true;
  result.enable_generic_output_options = true;

  return result;
}

po::options_description cli_options() {
  po::options_description cli_generic("Generic options");
  cli_generic.add_options()("help", "produce help message");
  cli_generic.add_options()(
      ",v", po::value<std::string>(),
      "log verbosity level (INFO, WARNING, ERROR, FATAL, OFF, 1-9)");

  return cli_generic;
}

struct recorder_configuration : cli_streams::configuration {
  recorder_configuration(int argc, char* argv[])
      : configuration(argc, argv, cli_configuration(), cli_options()) {}
};

}  // namespace

int main(int argc, char* argv[]) {
  recorder_configuration config{argc, argv};

  init_logging(argc, argv);

  boost::asio::io_service io_service;
  boost::asio::ssl::context ssl_context{boost::asio::ssl::context::sslv23};
  rtm_error_handler error_handler;

  std::shared_ptr<rtm::client> rtm_client = config.rtm_client(
      io_service, std::this_thread::get_id(), ssl_context, error_handler);
  std::string rtm_channel = config.rtm_channel();

  streams::publisher<satori::video::encoded_packet> source =
      config.decoded_publisher(io_service, rtm_client, rtm_channel,
                               image_pixel_format::RGB0)
      >> streams::signal_breaker({SIGINT, SIGTERM, SIGQUIT})
      >> streams::threaded_worker("input_buffer") >> streams::flatten()
      >> satori::video::encode_vp9(25) >> streams::threaded_worker("vp9_encoded_buffer")
      >> streams::flatten() >> streams::do_finally([&io_service, &rtm_client]() {
          io_service.post([&rtm_client]() {
            if (rtm_client) {
              if (auto ec = rtm_client->stop()) {
                LOG(ERROR) << "error stopping rtm client: " << ec.message();
              } else {
                LOG(INFO) << "rtm client was stopped";
              }
            }
          });
        });

  if (rtm_client) {
    if (auto ec = rtm_client->start()) {
      ABORT() << "error starting rtm client: " << ec.message();
    }
  }

  LOG(INFO) << "Starting recording...";

  source->subscribe(config.encoded_subscriber(rtm_client, io_service, rtm_channel));

  io_service.run();

  LOG(INFO) << "Recording is done";
}
