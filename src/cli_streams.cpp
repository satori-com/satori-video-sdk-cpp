#include <iostream>

#include "avutils.h"
#include "cli_streams.h"
#include "mkv_options.h"
#include "streams/asio_streams.h"
#include "streams/threaded_worker.h"
#include "video_metrics.h"
#include "video_streams.h"
#include "vp9_encoder.h"

namespace satori {
namespace video {
namespace cli_streams {

namespace {

po::options_description rtm_options() {
  po::options_description online("Satori RTM connection options");
  online.add_options()("endpoint", po::value<std::string>(), "app endpoint");
  online.add_options()("appkey", po::value<std::string>(), "app key");
  online.add_options()("port", po::value<std::string>()->default_value("443"), "port");

  return online;
}

po::options_description file_input_options(bool enable_batch_mode) {
  po::options_description file_sources("Input file options");
  file_sources.add_options()("input-video-file", po::value<std::string>(),
                             "input mp4,mkv,webm");
  file_sources.add_options()("input-replay-file", po::value<std::string>(), "input txt");
  file_sources.add_options()("loop", "Is file looped");

  if (enable_batch_mode) {
    file_sources.add_options()(
        "batch",
        "turns on batch analysis mode, where analysis of a single video frame might take "
        "longer than frame duration (file source only).");
  }

  return file_sources;
}

po::options_description camera_input_options() {
  po::options_description camera_options("Camera options");
  camera_options.add_options()("input-camera", "Is camera used as a source");

  return camera_options;
}

po::options_description generic_input_options() {
  po::options_description options("Generic input options");
  options.add_options()("time-limit", po::value<int>(),
                        "(seconds) if specified, bot will exit after given time elapsed");
  options.add_options()(
      "frames-limit", po::value<int>(),
      "(number) if specified, bot will exit after processing given number of frames");
  options.add_options()("input-resolution",
                        po::value<std::string>()->default_value("320x240"),
                        "(<width>x<height>|original) resolution of input video stream");
  options.add_options()("keep-proportions", po::value<bool>()->default_value(true),
                        "(bool) tells if original video stream resolution's proportion "
                        "should remain unchanged");

  return options;
}

po::options_description generic_output_options() {
  po::options_description options("Generic output options");
  options.add_options()("output-resolution",
                        po::value<std::string>()->default_value("320x240"),
                        "(<width>x<height>|original) resolution of output video stream");
  options.add_options()("keep-proportions", po::value<bool>()->default_value(true),
                        "(bool) tells if output video stream resolution's proportion "
                        "should remain unchanged");

  return options;
}

po::options_description url_input_options() {
  po::options_description url_options("URL options");
  url_options.add_options()("input-url", po::value<std::string>(), "Input video URL");
  return url_options;
}

po::options_description file_output_options() {
  po::options_description output_file_options("Output file options");
  output_file_options.add_options()("output-video-file", po::value<std::string>(),
                                    "Output video file");
  output_file_options.add_options()(
      "reserved-index-space", po::value<int>()->default_value(0),
      "(bytes) Tells how much space to reserve at the beginning of "
      "file for cues (indexes) to improve seeking. "
      "Typically 50000 is enough for one hour of video. "
      "For Matroska, if not specified (e.g. set to zero), "
      "cues will be written at the end of the file.");
  output_file_options.add_options()("segment-duration", po::value<int>(),
                                    "(seconds) Nearly fixed duration of output video "
                                    "file segments");

  return output_file_options;
}

po::options_description pool_mode_options() {
  po::options_description pool_mode_options("Pool mode options");
  pool_mode_options.add_options()(
      "pool", po::value<std::string>(),
      "Start program in pool mode, "
      "in this case program advertises its capacity "
      "on RTM channel and waits for job assignments from pool manager");
  pool_mode_options.add_options()("pool-job-type", po::value<std::string>(),
                                  "Pool job type supported by program");

  return pool_mode_options;
}

bool check_rtm_args_provided(const po::variables_map &vm) {
  return vm.count("endpoint") > 0 || vm.count("appkey") > 0
         || vm.count("input-channel") > 0 || vm.count("output-channel") > 0;
}

bool check_file_input_args_provided(const po::variables_map &vm) {
  return vm.count("input-video-file") > 0 || vm.count("input-replay-file") > 0;
}

bool check_camera_input_args_provided(const po::variables_map &vm) {
  return vm.count("input-camera") > 0;
}

bool check_url_input_args_provided(const po::variables_map &vm) {
  return vm.count("input-url") > 0;
}

bool check_file_output_args_provided(const po::variables_map &vm) {
  return vm.count("output-video-file") > 0;
}

bool check_pool_mode_args_provided(const po::variables_map &vm) {
  return vm.count("pool") > 0;
}

bool validate_rtm_args(const cli_options &opts, const po::variables_map &vm) {
  if (vm.count("endpoint") == 0) {
    std::cerr << "Missing --endpoint argument\n";
    return false;
  }
  if (vm.count("appkey") == 0) {
    std::cerr << "Missing --appkey argument\n";
    return false;
  }
  if (opts.enable_rtm_input && vm.count("input-channel") == 0 && vm.count("pool") == 0) {
    std::cerr << "Missing --input-channel or --pool (when available) argument\n";
    return false;
  }
  if (opts.enable_rtm_output && vm.count("output-channel") == 0) {
    std::cerr << "Missing --output-channel argument\n";
    return false;
  }
  if (vm.count("port") == 0) {
    std::cerr << "Missing --port argument\n";
    return false;
  }

  return true;
}

bool validate_input_file_args(const po::variables_map &vm) {
  if (vm.count("input-video-file") > 0 && vm.count("input-replay-file") > 9) {
    std::cerr << "--input-video-file and --input-replay-file are mutually exclusive\n";
    return false;
  }

  return true;
}

po::options_description to_boost(const cli_options &opts) {
  po::options_description options;

  if (opts.enable_rtm_input) {
    auto rtm = rtm_options();
    rtm.add_options()("input-channel", po::value<std::string>(), "input channel");
    options.add(rtm);
  }
  if (opts.enable_file_input) {
    options.add(file_input_options(opts.enable_file_batch_mode));
  }
  if (opts.enable_camera_input) {
    options.add(camera_input_options());
  }
  if (opts.enable_url_input) {
    options.add(url_input_options());
  }
  if (opts.enable_generic_input_options) {
    options.add(generic_input_options());
  }
  if (opts.enable_generic_output_options) {
    options.add(generic_output_options());
  }
  if (opts.enable_rtm_output) {
    auto rtm = rtm_options();
    rtm.add_options()("output-channel", po::value<std::string>(), "output channel");
    options.add(rtm);
  }
  if (opts.enable_file_output) {
    options.add(file_output_options());
  }
  if (opts.enable_pool_mode) {
    options.add(pool_mode_options());
  }

  return options;
}
}  // namespace

streams::publisher<encoded_packet> encoded_publisher(
    boost::asio::io_service &io, const std::shared_ptr<rtm::client> &client,
    const input_video_config &video_cfg) {
  if (video_cfg.input_channel) {
    return rtm_source(client, video_cfg.input_channel.get())
           >> report_video_metrics(video_cfg.input_channel.get())
           >> decode_network_stream()
           >> streams::threaded_worker("decoder_" + video_cfg.input_channel.get())
           >> streams::flatten();
  }

  if (video_cfg.input_video_file || video_cfg.input_replay_file) {
    streams::publisher<encoded_packet> source;
    if (video_cfg.input_video_file) {
      source = file_source(io, video_cfg.input_video_file.get(), video_cfg.loop,
                           video_cfg.batch);
    } else {
      auto replay_file = video_cfg.input_replay_file.get();
      source = network_replay_source(io, replay_file, video_cfg.batch)
               >> report_video_metrics(replay_file) >> decode_network_stream();
    }

    if (video_cfg.batch) {
      return source;
    }

    return std::move(source) >> streams::threaded_worker("input.encoded_buffer")
           >> streams::flatten();
  }

  if (video_cfg.input_camera) {
    const uint8_t fps = 25;                // FIXME: hardcoded value
    const uint8_t vp9_lag_in_frames = 25;  // FIXME: hardcoded value

    return camera_source(io, video_cfg.resolution, fps) >> encode_vp9(vp9_lag_in_frames);
  }

  if (video_cfg.input_url) {
    return url_source(video_cfg.input_url.get());
  }

  ABORT() << "should not happen";
}

streams::publisher<owned_image_packet> decoded_publisher(
    boost::asio::io_service &io, const std::shared_ptr<rtm::client> &client,
    const input_video_config &video_cfg, image_pixel_format pixel_format) {
  const auto resolution = avutils::parse_image_size(video_cfg.resolution);
  CHECK(resolution.ok()) << "bad resolution: " << video_cfg.resolution;

  streams::publisher<owned_image_packet> source =
      encoded_publisher(io, client, video_cfg)
      >> decode_image_frames(resolution.get(), pixel_format, video_cfg.keep_aspect_ratio);

  if (video_cfg.time_limit) {
    source =
        std::move(source) >> streams::asio::timer_breaker<owned_image_packet>(
                                 io, std::chrono::seconds(video_cfg.time_limit.get()));
  }

  if (video_cfg.frames_limit) {
    source = std::move(source) >> streams::take(video_cfg.frames_limit.get());
  }

  return source;
}

streams::subscriber<encoded_packet> &encoded_subscriber(
    boost::asio::io_service &io, const std::shared_ptr<rtm::client> &client,
    const output_video_config &config) {
  if (config.output_channel) {
    return rtm_sink(client, io, *config.output_channel);
  }

  if (config.output_path) {
    CHECK(config.reserved_index_space);

    mkv::format_options mkv_format_options;
    mkv_format_options.reserved_index_space = *config.reserved_index_space;
    return mkv_sink(*config.output_path, config.segment_duration, mkv_format_options);
  }

  ABORT() << "shouldn't happen";
}

configuration::configuration(int argc, char *argv[], cli_options options,
                             const po::options_description &custom_options)
    : _cli_options(options) {
  po::options_description description = to_boost(options);
  description.add(custom_options);

  try {
    po::store(po::parse_command_line(argc, argv, description), _vm);
    po::notify(_vm);
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl << description << std::endl;
    exit(1);
  }

  if (argc == 1 || _vm.count("help") > 0) {
    std::cerr << description << std::endl;
    exit(1);
  }

  if (!validate()) {
    exit(1);
  }

  if (_vm.count("config") > 0 && _vm.count("config-file") > 0) {
    std::cerr << "--config and --config-file options are mutually exclusive" << std::endl;
    exit(1);
  }
}

bool configuration::validate() const {
  const bool has_input_rtm_args =
      _cli_options.enable_rtm_input && check_rtm_args_provided(_vm);
  const bool has_input_file_args =
      _cli_options.enable_file_input && check_file_input_args_provided(_vm);
  const bool has_input_camera_args =
      _cli_options.enable_camera_input && check_camera_input_args_provided(_vm);

  if (((int)has_input_rtm_args + (int)has_input_file_args + (int)has_input_camera_args)
      > 1) {
    std::cerr << "Only one video source should be specified\n";
    return false;
  }

  if (_cli_options.enable_rtm_input || _cli_options.enable_file_input
      || _cli_options.enable_camera_input || _cli_options.enable_url_input) {
    if (!has_input_rtm_args && !has_input_file_args && !has_input_camera_args
        && !_cli_options.enable_url_input) {
      std::cerr << "Video source should be specified\n";
      return false;
    }
  }

  if (has_input_rtm_args && !validate_rtm_args(_cli_options, _vm)) {
    return false;
  }
  if (has_input_file_args && !validate_input_file_args(_vm)) {
    return false;
  }

  const bool has_output_rtm_args =
      _cli_options.enable_rtm_output && check_rtm_args_provided(_vm);
  const bool has_output_file_args =
      _cli_options.enable_file_output && check_file_output_args_provided(_vm);

  if (has_output_rtm_args && has_output_file_args) {
    std::cerr << "Only one video output should be specified\n";
    return false;
  }

  // TODO: use it more in this validation function
  const bool has_pool_mode_args =
      _cli_options.enable_pool_mode && check_pool_mode_args_provided(_vm);

  if (!has_pool_mode_args) {
    if (_cli_options.enable_rtm_output || _cli_options.enable_file_output) {
      if (!has_output_rtm_args && !has_output_file_args) {
        std::cerr << "Video output should be specified\n";
        return false;
      }
    }
  }

  if (has_input_rtm_args && has_output_rtm_args) {
    std::cerr << "RTM input and RTM output together are not supported currently\n";
    return false;
  }

  if (has_output_rtm_args && !validate_rtm_args(_cli_options, _vm)) {
    return false;
  }

  if (_cli_options.enable_generic_input_options) {
    std::string resolution = _vm["input-resolution"].as<std::string>();
    if (!avutils::parse_image_size(resolution).ok()) {
      std::cerr << "Unable to parse input resolution: " << resolution << "\n";
      return false;
    }
  }

  if (_cli_options.enable_generic_output_options) {
    std::string resolution = _vm["output-resolution"].as<std::string>();
    if (!avutils::parse_image_size(resolution).ok()) {
      std::cerr << "Unable to parse output resolution: " << resolution << "\n";
      return false;
    }
  }

  return true;
}

std::shared_ptr<rtm::client> configuration::rtm_client(
    boost::asio::io_service &io_service, std::thread::id io_thread_id,
    boost::asio::ssl::context &ssl_context,
    rtm::error_callbacks &rtm_error_callbacks) const {
  if (!_cli_options.enable_rtm_input && !_cli_options.enable_rtm_output) {
    return nullptr;
  }
  if (!check_rtm_args_provided(_vm)) {
    return nullptr;
  }

  const std::string endpoint = _vm["endpoint"].as<std::string>();
  const std::string port = _vm["port"].as<std::string>();
  const std::string appkey = _vm["appkey"].as<std::string>();

  return std::make_shared<rtm::thread_checking_client>(
      io_service, io_thread_id,
      std::make_unique<rtm::resilient_client>(
          io_service, io_thread_id,
          [endpoint, port, appkey, &io_service, &ssl_context,
           &rtm_error_callbacks](rtm::error_callbacks &callbacks) {
            return rtm::new_client(endpoint, port, appkey, io_service, ssl_context, 1,
                                   callbacks);
          },
          rtm_error_callbacks));
}

bool configuration::is_batch_mode() const {
  return _cli_options.enable_file_input && _cli_options.enable_file_batch_mode
         && _vm.count("batch") > 0;
}

streams::publisher<encoded_packet> configuration::encoded_publisher(
    boost::asio::io_service &io, const std::shared_ptr<rtm::client> &client) const {
  return cli_streams::encoded_publisher(io, client, input_video_config{_vm});
}

streams::publisher<owned_image_packet> configuration::decoded_publisher(
    boost::asio::io_service &io, const std::shared_ptr<rtm::client> &client,
    image_pixel_format pixel_format) const {
  CHECK(_cli_options.enable_generic_input_options
        || _cli_options.enable_generic_output_options);

  return cli_streams::decoded_publisher(io, client, input_video_config{_vm},
                                        pixel_format);
}

streams::subscriber<encoded_packet> &configuration::encoded_subscriber(
    boost::asio::io_service &io, const std::shared_ptr<rtm::client> &client) const {
  return cli_streams::encoded_subscriber(io, client, output_video_config{_vm});
}

input_video_config::input_video_config(const po::variables_map &vm)
    : input_channel(vm.count("input-channel") > 0 ? vm["input-channel"].as<std::string>()
                                                  : boost::optional<std::string>{}),
      batch(vm.count("batch") > 0),
      resolution(vm.count("input-resolution") > 0
                     ? vm["input-resolution"].as<std::string>()
                     : (vm.count("output-resolution") > 0
                            ? vm["output-resolution"].as<std::string>()
                            : "original")),
      keep_aspect_ratio(vm["keep-proportions"].as<bool>()),
      input_video_file(vm.count("input-video-file") > 0
                           ? vm["input-video-file"].as<std::string>()
                           : boost::optional<std::string>{}),
      input_replay_file(vm.count("input-replay-file") > 0
                            ? vm["input-replay-file"].as<std::string>()
                            : boost::optional<std::string>{}),
      input_url(vm.count("input-url") > 0 ? vm["input-url"].as<std::string>()
                                          : boost::optional<std::string>{}),
      input_camera(vm.count("input-camera") > 0),
      loop(vm.count("loop") > 0),
      time_limit(vm.count("time-limit") > 0 ? vm["time-limit"].as<int>()
                                            : boost::optional<int>{}),
      frames_limit(vm.count("frames-limit") > 0 ? vm["frames-limit"].as<int>()
                                                : boost::optional<int>{}) {}

input_video_config::input_video_config(const nlohmann::json &config)
    : input_channel(config.find("channel") != config.end()
                        ? config["channel"].get<std::string>()
                        : boost::optional<std::string>{}),
      batch(config.find("batch") != config.end()),
      resolution(config.find("resolution") != config.end()
                     ? config["resolution"].get<std::string>()
                     : "original"),
      keep_aspect_ratio(config.find("keep_proportions") != config.end()),
      input_video_file(config.find("input_video_file") != config.end()
                           ? config["input_video_file"].get<std::string>()
                           : boost::optional<std::string>{}),
      input_replay_file(config.find("input_replay_file") != config.end()
                            ? config["input_replay_file"].get<std::string>()
                            : boost::optional<std::string>{}),
      input_url(config.find("input_url") != config.end()
                    ? config["input_url"].get<std::string>()
                    : boost::optional<std::string>{}),
      input_camera(config.find("input_camera") != config.end()),
      loop(config.find("loop") != config.end()),
      time_limit(config.find("time_limit") != config.end()
                     ? config["time_limit"].get<long>()
                     : boost::optional<long>{}),
      frames_limit(config.find("frames_limit") != config.end()
                       ? config["frames_limit"].get<long>()
                       : boost::optional<long>{}) {}

output_video_config::output_video_config(const po::variables_map &vm)
    : output_channel{vm.count("output-channel") > 0
                         ? vm["output-channel"].as<std::string>()
                         : boost::optional<std::string>{}},
      output_path{vm.count("output-video-file") > 0
                      ? vm["output-video-file"].as<std::string>()
                      : boost::optional<std::string>{}},
      segment_duration{
          vm.count("segment-duration") > 0
              ? boost::optional<std::chrono::system_clock::duration>{std::chrono::seconds{
                    vm["segment-duration"].as<int>()}}
              : boost::optional<std::chrono::system_clock::duration>{}},
      reserved_index_space{vm.count("reserved-index-space") > 0
                               ? vm["reserved-index-space"].as<int>()
                               : boost::optional<int>{}} {}

output_video_config::output_video_config(const nlohmann::json &config)
    : output_channel{config.find("output-channel") != config.end()
                         ? config["output-channel"].get<std::string>()
                         : boost::optional<std::string>{}},
      output_path{config.find("output-video-file") != config.end()
                      ? config["output-video-file"].get<std::string>()
                      : boost::optional<std::string>{}},
      segment_duration{
          config.find("segment-duration") != config.end()
              ? boost::optional<std::chrono::system_clock::duration>{std::chrono::seconds{
                    config["segment-duration"].get<int>()}}
              : boost::optional<std::chrono::system_clock::duration>{}},
      reserved_index_space{config.find("reserved-index-space") != config.end()
                               ? config["reserved-index-space"].get<int>()
                               : 0} {}
}  // namespace cli_streams
}  // namespace video
}  // namespace satori
