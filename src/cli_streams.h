#pragma once

#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <chrono>
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
  bool enable_pool_mode{false};
};

struct input_video_config {
  explicit input_video_config(const po::variables_map &vm);
  explicit input_video_config(const nlohmann::json &config);

  const bool batch;
  const std::string resolution;
  const bool keep_proportions;
  const boost::optional<std::string> input_video_file;
  const boost::optional<std::string> input_replay_file;
  const boost::optional<std::string> input_url;
  const boost::optional<std::string> input_channel;
  const bool input_camera;
  const bool loop;
  const boost::optional<int> time_limit;
  const boost::optional<int> frames_limit;
};

struct output_video_config {
  explicit output_video_config(const po::variables_map &vm);
  explicit output_video_config(const nlohmann::json &config);

  const boost::optional<std::string> channel;
  const boost::optional<std::string> video_file;
  const boost::optional<std::chrono::system_clock::duration> segment_duration;
  const boost::optional<int> reserved_index_space;
};

streams::publisher<encoded_packet> encoded_publisher(
    boost::asio::io_service &io, const std::shared_ptr<rtm::client> &client,
    const input_video_config &video_cfg);

streams::publisher<owned_image_packet> decoded_publisher(
    boost::asio::io_service &io, const std::shared_ptr<rtm::client> &client,
    const input_video_config &video_cfg, image_pixel_format pixel_format);

streams::subscriber<encoded_packet> &encoded_subscriber(
    boost::asio::io_service &io, const std::shared_ptr<rtm::client> &client,
    const output_video_config &config);

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

  bool is_batch_mode() const;

  streams::publisher<encoded_packet> encoded_publisher(
      boost::asio::io_service &io, const std::shared_ptr<rtm::client> &client) const;

  streams::publisher<owned_image_packet> decoded_publisher(
      boost::asio::io_service &io, const std::shared_ptr<rtm::client> &client,
      image_pixel_format pixel_format) const;

  streams::subscriber<encoded_packet> &encoded_subscriber(
      boost::asio::io_service &io, const std::shared_ptr<rtm::client> &client) const;

  metrics_config metrics() const { return metrics_config{_vm}; }

 protected:
  po::variables_map _vm;
  cli_options _cli_options;
};

}  // namespace cli_streams
}  // namespace video
}  // namespace satori
