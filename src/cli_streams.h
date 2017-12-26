#pragma once

#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <memory>
#include <string>
#include <thread>

#include "data.h"
#include "metrics.h"
#include "rtm_client.h"
#include "streams/streams.h"

namespace satori {
namespace video {
namespace cli_streams {

namespace po = boost::program_options;

struct cli_options {
  bool enable_rtm_input{false};
  bool enable_file_input{false};
  bool enable_camera_input{false};
  bool enable_generic_input_options{false};
  bool enable_generic_output_options{false};
  bool enable_rtm_output{false};
  bool enable_file_output{false};
  bool enable_file_batch_mode{false};
  bool enable_url_input{false};
};

struct configuration {
 public:
  configuration(int argc, char *argv[], cli_options options,
                const po::options_description &custom_options);

  virtual ~configuration() = default;

  bool validate() const;

  std::shared_ptr<rtm::client> rtm_client(
      boost::asio::io_service &io_service, std::thread::id io_thread_id,
      boost::asio::ssl::context &ssl_context,
      rtm::error_callbacks &rtm_error_callbacks) const;

  std::string rtm_channel() const;

  bool is_batch_mode() const;

  streams::publisher<encoded_packet> encoded_publisher(
      boost::asio::io_service &io_service, const std::shared_ptr<rtm::client> &client,
      const std::string &channel) const;

  streams::publisher<owned_image_packet> decoded_publisher(
      boost::asio::io_service &io_service, const std::shared_ptr<rtm::client> &client,
      const std::string &channel, image_pixel_format pixel_format) const;

  streams::subscriber<encoded_packet> &encoded_subscriber(
      const std::shared_ptr<rtm::client> &client, boost::asio::io_service &io_service,
      const std::string &channel) const;

  metrics_config metrics() const { return metrics_config{_vm}; }

 protected:
  po::variables_map _vm;
  cli_options _cli_options;
};

}  // namespace cli_streams
}  // namespace video
}  // namespace satori
