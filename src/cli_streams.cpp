#include <iostream>

#include "cli_streams.h"
#include "video_streams.h"
#include "worker.h"

namespace rtm {
namespace video {
namespace cli_streams {

namespace {

po::options_description rtm_options() {
  po::options_description online("Satori RTM connection options");
  online.add_options()("endpoint", po::value<std::string>(), "app endpoint");
  online.add_options()("appkey", po::value<std::string>(), "app key");
  online.add_options()("port", po::value<std::string>(), "port");
  online.add_options()("channel", po::value<std::string>(), "channel");

  return online;
}

po::options_description file_input_options(bool enable_batch_mode) {
  po::options_description file_sources("Input file options");
  file_sources.add_options()("input-video-file", po::value<std::string>(),
                             "input mp4,mkv,webm");
  file_sources.add_options()("input-replay-file", po::value<std::string>(), "input txt");
  file_sources.add_options()("loop", "Is file looped");

  if (enable_batch_mode)
    file_sources.add_options()(
        "batch",
        "turns on batch analysis mode, where analysis of a single video frame might take "
        "longer than frame duration (file source only).");

  return file_sources;
}

po::options_description camera_input_options() {
  po::options_description camera_options("Camera options");
  camera_options.add_options()("input-camera", "Is camera used as a source");
  camera_options.add_options()("camera-dimensions",
                               po::value<std::string>()->default_value("320x240"),
                               "Camera dimensions");

  return camera_options;
}

po::options_description file_output_options() {
  po::options_description output_file_options("Output file options");
  output_file_options.add_options()("output-video-file", po::value<std::string>(),
                                    "Output video file");

  return output_file_options;
}

bool check_rtm_args_provided(const po::variables_map &vm) {
  return vm.count("endpoint") || vm.count("port") || vm.count("appkey")
         || vm.count("channel");
}

bool check_file_input_args_provided(const po::variables_map &vm) {
  return vm.count("input-video-file") || vm.count("input-replay-file");
}

bool check_camera_input_args_provided(const po::variables_map &vm) {
  return vm.count("input-camera");
}

bool check_file_output_args_provided(const po::variables_map &vm) {
  return vm.count("output-video-file");
}

bool validate_rtm_args(const po::variables_map &vm) {
  if (!vm.count("endpoint")) {
    std::cerr << "Missing --endpoint argument\n";
    return false;
  }
  if (!vm.count("appkey")) {
    std::cerr << "Missing --appkey argument\n";
    return false;
  }
  if (!vm.count("channel")) {
    std::cerr << "Missing --channel argument\n";
    return false;
  }
  if (!vm.count("port")) {
    std::cerr << "Missing --port argument\n";
    return false;
  }

  return true;
}

bool validate_file_input_args(const po::variables_map &vm) {
  if (vm.count("input-video-file") && vm.count("input-replay-file")) {
    std::cerr << "--input-video-file and --input-replay-file are mutually exclusive\n";
    return false;
  }

  return true;
}
}  // namespace

constexpr size_t network_buffer_size = 1024;
constexpr size_t encoded_buffer_size = 10;

po::options_description configuration::to_boost() const {
  po::options_description options;

  if (enable_rtm_input) options.add(rtm_options());
  if (enable_file_input) options.add(file_input_options(enable_file_batch_mode));
  if (enable_camera_input) options.add(camera_input_options());

  if (enable_rtm_output) options.add(rtm_options());
  if (enable_file_output) options.add(file_output_options());

  return options;
}

bool configuration::validate(const po::variables_map &vm) const {
  const bool has_input_rtm_args = enable_rtm_input && check_rtm_args_provided(vm);
  const bool has_input_file_args =
      enable_file_input && check_file_input_args_provided(vm);
  const bool has_input_camera_args =
      enable_camera_input && check_camera_input_args_provided(vm);

  if (((int)has_input_rtm_args + (int)has_input_file_args + (int)has_input_camera_args)
      > 1) {
    std::cerr << "Only one video source should be specified\n";
    return false;
  }

  if (enable_rtm_input || enable_file_input || enable_camera_input) {
    if (!has_input_rtm_args && !has_input_file_args && !has_input_camera_args) {
      std::cerr << "Video source should be specified\n";
      return false;
    }
  }

  if (has_input_rtm_args && !validate_rtm_args(vm)) return false;
  if (has_input_file_args && !validate_file_input_args(vm)) return false;

  const bool has_output_rtm_args = enable_rtm_output && check_rtm_args_provided(vm);
  const bool has_output_file_args =
      enable_file_output && check_file_output_args_provided(vm);

  if (has_output_rtm_args && has_output_file_args) {
    std::cerr << "Only one video output should be specified\n";
    return false;
  }

  if (enable_rtm_output || enable_file_output) {
    if (!has_output_rtm_args && !has_output_file_args) {
      std::cerr << "Video output should be specified\n";
      return false;
    }
  }

  if (has_input_rtm_args && has_output_rtm_args) {
    std::cerr << "RTM input and RTM output together are not supported currently\n";
    return false;
  }

  if (has_output_rtm_args && !validate_rtm_args(vm)) return false;

  return true;
}

std::shared_ptr<rtm::client> configuration::rtm_client(
    const po::variables_map &vm, boost::asio::io_service &io_service,
    boost::asio::ssl::context &ssl_context, error_callbacks &rtm_error_callbacks) const {
  if (!enable_rtm_input && !enable_rtm_output) return nullptr;
  if (!check_rtm_args_provided(vm)) return nullptr;

  const std::string endpoint = vm["endpoint"].as<std::string>();
  const std::string port = vm["port"].as<std::string>();
  const std::string appkey = vm["appkey"].as<std::string>();

  return std::make_shared<resilient_client>(
      [endpoint, port, appkey, &io_service, &ssl_context, &rtm_error_callbacks]() {
        return rtm::new_client(endpoint, port, appkey, io_service, ssl_context, 1,
                               rtm_error_callbacks);
      });
}

std::string configuration::rtm_channel(const po::variables_map &vm) const {
  if (!enable_rtm_input && !enable_rtm_output) return "";
  if (!check_rtm_args_provided(vm)) return "";

  return vm["channel"].as<std::string>();
}

bool configuration::is_batch_mode(const po::variables_map &vm) const {
  return enable_file_input && enable_file_batch_mode && vm.count("batch");
}

streams::publisher<encoded_packet> configuration::encoded_publisher(
    const po::variables_map &vm, boost::asio::io_service &io_service,
    const std::shared_ptr<rtm::client> client, const std::string &channel,
    bool network_buffer) const {
  const bool has_input_rtm_args = enable_rtm_input && check_rtm_args_provided(vm);
  const bool has_input_file_args =
      enable_file_input && check_file_input_args_provided(vm);
  const bool has_input_camera_args =
      enable_camera_input && check_camera_input_args_provided(vm);

  if (has_input_rtm_args) {
    streams::publisher<network_packet> source = rtm::video::rtm_source(client, channel);

    if (network_buffer)
      source = std::move(source)
               >> buffered_worker("input.network_buffer", network_buffer_size);

    return std::move(source) >> rtm::video::decode_network_stream()
           >> buffered_worker("input.encoded_buffer", encoded_buffer_size);
  } else if (has_input_file_args) {
    const bool batch = enable_file_batch_mode && vm.count("batch");

    streams::publisher<encoded_packet> source;
    if (vm.count("input-video-file")) {
      source = rtm::video::file_source(
          io_service, vm["input-video-file"].as<std::string>(), vm.count("loop"), batch);
    } else {
      source = rtm::video::network_replay_source(
                   io_service, vm["input-replay-file"].as<std::string>(), batch)
               >> rtm::video::decode_network_stream();
    }

    if (batch) {
      return std::move(source);
    } else {
      return std::move(source)
             >> buffered_worker("input.encoded_buffer", encoded_buffer_size);
    }
  } else if (has_input_camera_args) {
    return rtm::video::camera_source(io_service,
                                     vm["camera-dimensions"].as<std::string>());
  } else {
    BOOST_VERIFY(false);
    exit(1);
  }
}

streams::subscriber<encoded_packet> &configuration::encoded_subscriber(
    const po::variables_map &vm, const std::shared_ptr<rtm::client> client,
    const std::string &channel) const {
  const bool has_output_rtm_args = enable_rtm_output && check_rtm_args_provided(vm);
  const bool has_output_file_args =
      enable_file_output && check_file_output_args_provided(vm);

  if (has_output_rtm_args) {
    return rtm::video::rtm_sink(client, channel);
  } else if (has_output_file_args) {
    return rtm::video::mkv_sink(vm["output-video-file"].as<std::string>());
  } else {
    BOOST_VERIFY(false);
    exit(1);
  }
}

}  // namespace cli_streams
}  // namespace video
}  // namespace rtm
