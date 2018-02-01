#include "bot_environment.h"

#include <algorithm>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <gsl/gsl>
#include <json.hpp>

#include "avutils.h"
#include "bot_instance.h"
#include "bot_instance_builder.h"
#include "logging_impl.h"
#include "ostream_sink.h"
#include "rtm_streams.h"
#include "signal_utils.h"
#include "streams/asio_streams.h"
#include "streams/signal_breaker.h"
#include "streams/threaded_worker.h"
#include "tcmalloc.h"

namespace satori {
namespace video {
namespace {

using variables_map = boost::program_options::variables_map;

po::options_description bot_custom_options() {
  po::options_description generic("Generic options");
  generic.add_options()("help", "produce help message");
  generic.add_options()(",v", po::value<std::string>(),
                        "log verbosity level (INFO, WARNING, ERROR, FATAL, OFF, 1-9)");

  po::options_description bot_configuration_options("Bot configuration options");
  bot_configuration_options.add_options()(
      "id", po::value<std::string>()->default_value(""), "bot id");
  bot_configuration_options.add_options()("config-file", po::value<std::string>(),
                                          "(json) bot config file");
  bot_configuration_options.add_options()("config", po::value<std::string>(),
                                          "(json) bot config");

  po::options_description bot_execution_options("Bot execution options");
  bot_execution_options.add_options()(
      "analysis-file", po::value<std::string>(),
      "saves analysis messages to a file instead of sending to a channel");
  bot_execution_options.add_options()(
      "debug-file", po::value<std::string>(),
      "saves debug messages to a file instead of sending to a channel");

  po::options_description bot_as_a_service_options("Bot as a service options");
  bot_as_a_service_options.add_options()(
      "pool", po::value<std::string>(),
      "Start bot as a service for a given pool, "
      "in this case bot advertises its capacity "
      "on RTM channel and listens for VMGR assignations");

  return bot_configuration_options.add(bot_execution_options)
      .add(metrics_options())
      .add(generic)
      .add(bot_as_a_service_options);
}

cli_streams::cli_options bot_cli_cfg() {
  cli_streams::cli_options cli_cfg;
  cli_cfg.enable_rtm_input = true;
  cli_cfg.enable_file_input = true;
  cli_cfg.enable_camera_input = true;
  cli_cfg.enable_generic_input_options = true;
  cli_cfg.enable_url_input = true;
  cli_cfg.enable_file_batch_mode = true;
  return cli_cfg;
}

nlohmann::json init_config(const po::variables_map& vm) {
  if (vm.count("config") == 0 && vm.count("config-file") == 0) {
    return nullptr;
  }

  nlohmann::json json_config;
  std::string config_string;

  if (vm.count("config-file") > 0) {
    std::ifstream config_file(vm["config-file"].as<std::string>());
    config_string = std::string((std::istreambuf_iterator<char>(config_file)),
                                std::istreambuf_iterator<char>());
  } else {
    config_string = vm["config"].as<std::string>();
  }

  try {
    json_config = nlohmann::json::parse(config_string);
  } catch (const nlohmann::json::parse_error& e) {
    std::cerr << "Can't parse config: " << e.what() << "\nArg: " << config_string
              << std::endl;
    exit(1);
  }

  return json_config;
}
}  // namespace

bot_environment& bot_environment::instance() {
  static bot_environment env;
  return env;
}

void bot_environment::register_bot(const multiframe_bot_descriptor& bot) {
  _bot_descriptor = bot;
}

void bot_environment::operator()(const owned_image_metadata& /*metadata*/) {}

void bot_environment::operator()(const owned_image_frame& /*frame*/) {}

void bot_environment::operator()(struct bot_message& msg) {
  switch (msg.kind) {
    case bot_message_kind::ANALYSIS:
      _analysis_sink->on_next(std::move(msg.data));
      break;
    case bot_message_kind::CONTROL:
      _control_sink->on_next(std::move(msg.data));
      break;
    case bot_message_kind::DEBUG:
      _debug_sink->on_next(std::move(msg.data));
      break;
  }
}

struct env_configuration : cli_streams::configuration {
  env_configuration(int argc, char* argv[])
      : configuration(argc, argv, bot_cli_cfg(), bot_custom_options()) {}

  bot_configuration bot_config() const { return bot_configuration{_vm}; }
  boost::optional<std::string> pool() const {
    return _vm.count("pool") > 0 ? _vm["pool"].as<std::string>()
                                 : boost::optional<std::string>{};
  }
  std::string id() const { return _vm["id"].as<std::string>(); }
};

bot_configuration::bot_configuration(const po::variables_map& vm)
    : id(vm["id"].as<std::string>()),
      analysis_file(vm.count("analysis-file") > 0 ? vm["analysis-file"].as<std::string>()
                                                  : boost::optional<std::string>{}),
      debug_file(vm.count("debug-file") > 0 ? vm["debug-file"].as<std::string>()
                                            : boost::optional<std::string>{}),
      video_cfg(vm),
      bot_config(init_config(vm)) {}

bot_configuration::bot_configuration(const nlohmann::json& config)
    : id(config["id"].get<std::string>()),
      analysis_file(config.find("analysis_file") != config.end()
                        ? config["analysis_file"].get<std::string>()
                        : boost::optional<std::string>{}),
      debug_file(config.find("debug_file") != config.end()
                     ? config["debug_file"].get<std::string>()
                     : boost::optional<std::string>{}),
      video_cfg(config),
      bot_config(config.find("config") != config.end() ? config["config"]
                                                       : nlohmann::json(nullptr)) {}

int bot_environment::main(int argc, char* argv[]) {
  init_tcmalloc();
  init_logging(argc, argv);

  env_configuration config{argc, argv};

  const bool batch = config.is_batch_mode();
  const std::string id = config.id();

  boost::asio::ssl::context ssl_context{boost::asio::ssl::context::sslv23};

  _rtm_client =
      config.rtm_client(_io_service, std::this_thread::get_id(), ssl_context, *this);
  if (_rtm_client) {
    if (auto ec = _rtm_client->start()) {
      ABORT() << "error starting rtm client: " << ec.message();
    }
  }
  _metrics_config = config.metrics();

  auto start = [config, this]() {
    if (!config.pool()) {
      start_bot(config.bot_config());
    } else {
      std::string pool = config.pool().get();
      std::string job_type = config.id();

      auto job_controller =
          new pool_job_controller(_io_service, pool, job_type, 1, _rtm_client, *this);

      // Kubernetes sends SIGTERM, and then SIGKILL after 30 seconds
      // https://kubernetes.io/docs/concepts/workloads/pods/pod/#termination-of-pods
      signal::register_handler({SIGINT, SIGTERM, SIGQUIT}, [job_controller](int signal) {
        LOG(INFO) << "Got signal #" << signal;
        job_controller->shutdown();
      });

      job_controller->start();
    }
  };

  if (!batch) {
    LOG(INFO) << "entering asio loop";
    _io_service.post(start);
    auto n = _io_service.run();
    LOG(INFO) << "asio loop exited, executed " << n << " handlers";

    while (!_finished) {
      // batch mode has no threads
      LOG(INFO) << "waiting for all threads to finish...";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  } else {
    start();
  }

  if (_rtm_client) {
    _io_service.post([rtm_client = _rtm_client]() {
      if (auto ec = rtm_client->stop()) {
        LOG(ERROR) << "error stopping rtm client: " << ec.message();
      } else {
        LOG(INFO) << "rtm client was stopped";
      }
    });
  }
  return 0;
}

void bot_environment::start_bot(const bot_configuration& config) {
  _metrics_config.push_job = config.id;
  init_metrics(_metrics_config, _io_service);
  expose_metrics(_rtm_client.get());

  const bool batch = config.video_cfg.batch;
  bot_instance_builder builder =
      bot_instance_builder{_bot_descriptor}
          .set_execution_mode(batch ? execution_mode::BATCH : execution_mode::LIVE)
          .set_bot_id(config.id)
          .set_config(config.bot_config);

  _bot_instance = builder.build();
  auto single_frame_source = cli_streams::decoded_publisher(
      _io_service, _rtm_client, config.video_cfg, _bot_descriptor.pixel_format);
  if (!batch) {
    _source =
        std::move(single_frame_source) >> streams::threaded_worker("processing_worker");
  } else {
    _source =
        std::move(single_frame_source) >> streams::map([](owned_image_packet&& pkt) {
          std::queue<owned_image_packet> q;
          q.push(pkt);
          return q;
        });
  }

  if (config.analysis_file) {
    std::string analysis_file = config.analysis_file.get();
    LOG(INFO) << "saving analysis output to " << analysis_file;
    _analysis_file = std::make_unique<std::ofstream>(analysis_file.c_str());
    _analysis_sink = &streams::ostream_sink(*_analysis_file);
  } else if (_rtm_client) {
    _analysis_sink =
        &rtm::sink(_rtm_client, _io_service,
                   config.video_cfg.input_channel.get() + analysis_channel_suffix);
  } else {
    _analysis_sink = &streams::ostream_sink(std::cout);
  }

  if (config.debug_file) {
    std::string debug_file = config.debug_file.get();
    LOG(INFO) << "saving debug output to " << debug_file;
    _debug_file = std::make_unique<std::ofstream>(debug_file.c_str());
    _debug_sink = &streams::ostream_sink(*_debug_file);
  } else if (_rtm_client) {
    _debug_sink = &rtm::sink(_rtm_client, _io_service,
                             config.video_cfg.input_channel.get() + debug_channel_suffix);
  } else {
    _debug_sink = &streams::ostream_sink(std::cerr);
  }

  if (_rtm_client) {
    _control_sink = &rtm::sink(_rtm_client, _io_service, control_channel);
    _control_source =
        rtm::channel(_rtm_client, control_channel, {})
        >> streams::map([](rtm::channel_data&& t) { return std::move(t.payload); });
  } else {
    _control_sink = &streams::ostream_sink(std::cout);
    _control_source = streams::publishers::empty<nlohmann::json>();
  }

  _finished = false;
  _multiframes_counter = 0;

  _source = std::move(_source) >> streams::signal_breaker({SIGINT, SIGTERM, SIGQUIT})
            >> streams::map([& multiframes_counter = _multiframes_counter](
                   std::queue<owned_image_packet> && pkt) mutable {
                multiframes_counter++;
                constexpr int period = 100;
                if ((multiframes_counter % period) == 0) {
                  LOG(INFO) << "Processed " << multiframes_counter << " multiframes";
                }
                return pkt;
              })
            >> streams::do_finally([this]() {
                _finished = true;

                _io_service.post([this]() { stop_metrics(); });
              });

  auto bot_input_stream = streams::publishers::merge<bot_input>(
      std::move(_control_source)
          >> streams::map([](nlohmann::json&& t) { return bot_input{t}; }),
      std::move(_source) >> streams::map([](std::queue<owned_image_packet>&& p) {
        return bot_input{p};
      }));

  auto bot_output_stream = std::move(bot_input_stream) >> _bot_instance->run_bot();

  bot_output_stream->process([this](bot_output&& o) { boost::apply_visitor(*this, o); });
}

void bot_environment::add_job(const nlohmann::json& job) {
  CHECK(_job.is_null()) << "Can't subscribe to more than one channel";
  _job = job;
  start_bot(bot_configuration{_job});
}

void bot_environment::remove_job(const nlohmann::json& job) {
  LOG(ERROR) << "Requested remove for the following job: " << job;
  ABORT() << "Removing jobs is not supported";
}

nlohmann::json bot_environment::list_jobs() const {
  nlohmann::json jobs = nlohmann::json::array();
  if (!_job.is_null()) {
    jobs.emplace_back(_job);
  }
  return jobs;
}

void bot_environment::on_error(std::error_condition ec) {
  ABORT() << "rtm error: " << ec.message();
}

}  // namespace video
}  // namespace satori
