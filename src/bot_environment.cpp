#include "bot_environment.h"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <algorithm>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <fstream>

#include "avutils.h"
#include "bot_instance.h"
#include "cbor_json.h"
#include "cbor_tools.h"
#include "cli_streams.h"
#include "logging_impl.h"
#include "metrics.h"
#include "rtm_streams.h"
#include "streams/asio_streams.h"
#include "streams/signal_breaker.h"

namespace satori {
namespace video {
namespace {

using variables_map = boost::program_options::variables_map;

variables_map parse_command_line(int argc, char* argv[],
                                 const cli_streams::configuration& cli_cfg) {
  namespace po = boost::program_options;

  po::options_description generic("Generic options");
  generic.add_options()("help", "produce help message");
  generic.add_options()(",v", po::value<std::string>(),
                        "log verbosity level (INFO, WARNING, ERROR, FATAL, OFF, 1-9)");
  generic.add_options()("metrics-bind-address",
                        po::value<std::string>()->default_value(""),
                        "socket bind address:port for metrics server");

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

  po::options_description cli_options = cli_cfg.to_boost();
  cli_options.add(bot_configuration_options).add(bot_execution_options).add(generic);

  po::variables_map vm;

  try {
    po::store(po::parse_command_line(argc, argv, cli_options), vm);
    po::notify(vm);
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    std::cerr << cli_options << std::endl;
    exit(1);
  }

  if (argc == 1 || vm.count("help") > 0) {
    std::cerr << cli_options << std::endl;
    exit(1);
  }

  if (!cli_cfg.validate(vm)) {
    exit(1);
  }

  if (vm.count("config") > 0 && vm.count("config-file") > 0) {
    std::cerr << "--config and --config-file options are mutually exclusive" << std::endl;
    exit(1);
  }

  return vm;
}

// todo: move this reusable class out.
struct file_cbor_dump_observer : public streams::observer<cbor_item_t*> {
  explicit file_cbor_dump_observer(std::ostream& out) : _out(out) {}

  void on_next(cbor_item_t*&& t) override {
    _out << t << std::endl;
    cbor_decref(&t);
  }
  void on_error(std::error_condition ec) override {
    LOG(ERROR) << "ERROR: " << ec.message();
    delete this;
  }

  void on_complete() override { delete this; }

  std::ostream& _out;
};

cbor_item_t* configure_command(cbor_item_t* config) {
  cbor_item_t* cmd = cbor_new_definite_map(2);
  cbor_map_add(cmd, {cbor_move(cbor_build_string("action")),
                     cbor_move(cbor_build_string("configure"))});
  cbor_map_add(cmd, {cbor_move(cbor_build_string("body")), config});
  return cmd;
}

}  // namespace

cbor_item_t* read_config_from_file(const std::string& config_file) {
  FILE* fp = fopen(config_file.c_str(), "r");
  if (fp == nullptr) {
    std::cerr << "Can't read config file " << config_file << ": " << strerror(errno)
              << std::endl;
    exit(1);
  }
  auto file_closer = gsl::finally([&fp]() { fclose(fp); });

  char readBuffer[65536];
  rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
  rapidjson::Document d;
  rapidjson::ParseResult ok = d.ParseStream(is);

  if (!ok) {
    std::cerr << "Config parse error at offset " << ok.Offset() << ": "
              << GetParseError_En(ok.Code()) << std::endl;
    exit(1);
  }

  return json_to_cbor(d);
}

cbor_item_t* read_config_from_arg(const std::string& arg) {
  rapidjson::Document d;
  rapidjson::ParseResult ok = d.Parse(arg.c_str());

  if (!ok) {
    std::cerr << "Config parse error at offset " << ok.Offset() << ": "
              << GetParseError_En(ok.Code()) << std::endl;
    exit(1);
  }

  return json_to_cbor(d);
}

void bot_environment::process_config(cbor_item_t* config) {
  if (!_bot_descriptor.ctrl_callback) {
    if (config == nullptr) return;
    std::cerr << "Bot control handler was not provided" << std::endl;
    exit(1);
  }

  if (config == nullptr) config = cbor_new_definite_map(0);

  cbor_item_t* cmd = configure_command(config);
  auto cbor_deleter = gsl::finally([&config, &cmd]() {
    cbor_decref(&config);
    cbor_decref(&cmd);
  });

  cbor_item_t* response = _bot_descriptor.ctrl_callback(*_bot_instance, cmd);
  if (response != nullptr) {
    _bot_instance->queue_message(bot_message_kind::DEBUG, cbor_move(response),
                                 frame_id{0, 0});
  }
}

bot_environment& bot_environment::instance() {
  static bot_environment env;
  return env;
}

void bot_environment::register_bot(const bot_descriptor& bot) { _bot_descriptor = bot; }

void bot_environment::send_messages(std::list<struct bot_message>&& messages) {
  for (auto&& msg : messages) {
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

  messages.clear();
}

int bot_environment::main(int argc, char* argv[]) {
  cli_streams::configuration cli_cfg;
  cli_cfg.enable_rtm_input = true;
  cli_cfg.enable_file_input = true;
  cli_cfg.enable_generic_input_options = true;
  cli_cfg.enable_file_batch_mode = true;

  auto cmd_args = parse_command_line(argc, argv, cli_cfg);
  init_logging(argc, argv);

  boost::asio::io_service io_service;

  std::string metrics_bind_address = cmd_args["metrics-bind-address"].as<std::string>();
  if (!metrics_bind_address.empty()) {
    expose_metrics(metrics_bind_address);
    report_process_metrics(io_service);
  }

  const std::string id = cmd_args["id"].as<std::string>();
  const bool batch = cmd_args.count("batch") > 0;
  _bot_instance = std::make_shared<bot_instance>(
      id, batch ? execution_mode::BATCH : execution_mode::LIVE, _bot_descriptor, *this);

  cbor_item_t* bot_config{nullptr};
  if (cmd_args.count("config-file") > 0) {
    bot_config = read_config_from_file(cmd_args["config-file"].as<std::string>());
  } else if (cmd_args.count("config") > 0) {
    bot_config = read_config_from_arg(cmd_args["config"].as<std::string>());
  }

  process_config(bot_config);

  boost::asio::ssl::context ssl_context{boost::asio::ssl::context::sslv23};

  _rtm_client = cli_cfg.rtm_client(cmd_args, io_service, ssl_context, *this);
  if (_rtm_client) {
    if (auto ec = _rtm_client->start()) {
      ABORT() << "error starting rtm client: " << ec.message();
    }
  }

  const std::string channel = cli_cfg.rtm_channel(cmd_args);
  const bool batch_mode = cli_cfg.is_batch_mode(cmd_args);

  if (cmd_args.count("analysis-file") > 0) {
    std::string analysis_file = cmd_args["analysis-file"].as<std::string>();
    LOG(INFO) << "saving analysis output to " << analysis_file;
    _analysis_file = std::make_unique<std::ofstream>(analysis_file.c_str());
    _analysis_sink = new file_cbor_dump_observer(*_analysis_file);
  } else if (_rtm_client) {
    _analysis_sink = &rtm::cbor_sink(_rtm_client, channel + analysis_channel_suffix);
  } else {
    _analysis_sink = new file_cbor_dump_observer(std::cout);
  }

  if (cmd_args.count("debug-file") > 0) {
    std::string debug_file = cmd_args["debug-file"].as<std::string>();
    LOG(INFO) << "saving debug output to " << debug_file;
    _debug_file = std::make_unique<std::ofstream>(debug_file.c_str());
    _debug_sink = new file_cbor_dump_observer(*_debug_file);
  } else if (_rtm_client) {
    _debug_sink = &rtm::cbor_sink(_rtm_client, channel + debug_channel_suffix);
  } else {
    _debug_sink = new file_cbor_dump_observer(std::cerr);
  }

  streams::publisher<cbor_item_t*> control_source;

  if (_rtm_client) {
    _control_sink = &rtm::cbor_sink(_rtm_client, control_channel);
    control_source = rtm::cbor_channel(_rtm_client, control_channel, {});
  } else {
    _control_sink = new file_cbor_dump_observer(std::cout);
    control_source = streams::publishers::empty<cbor_item_t*>();
  }

  bool finished{false};
  int frames_count = 0;

  streams::publisher<owned_image_packet> video_source =
      cli_cfg.decoded_publisher(cmd_args, io_service, _rtm_client, channel,
                                _bot_descriptor.pixel_format)
      >> streams::signal_breaker<owned_image_packet>({SIGINT, SIGTERM, SIGQUIT})
      >> streams::do_finally([this, &finished]() {
          finished = true;
          _bot_instance->stop();
          if (_rtm_client) {
            auto ec = _rtm_client->stop();
            if (ec) {
              LOG(ERROR) << "error stopping rtm client: " << ec.message();
            }
          }
        });

  _bot_instance->start(std::move(video_source), std::move(control_source));

  if (!batch_mode) {
    LOG(INFO) << "entering asio loop";
    auto n = io_service.run();
    LOG(INFO) << "asio loop exited, executed " << n << " handlers";

    while (!finished) {
      // batch mode has no threads
      LOG(INFO) << "waiting for all threads to finish...";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  return 0;
}

void bot_environment::on_error(std::error_condition ec) {
  ABORT() << "rtm error: " << ec.message();
}

}  // namespace video
}  // namespace satori
