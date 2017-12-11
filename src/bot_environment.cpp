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
#include <gsl/gsl>

#include "bot_instance.h"
#include "cbor_json.h"
#include "cbor_tools.h"  // FIXME: this one is implicitly used by file_cbor_dump_observer
#include "cli_streams.h"
#include "logging_impl.h"
#include "metrics.h"
#include "rtm_streams.h"
#include "streams/asio_streams.h"
#include "streams/signal_breaker.h"
#include "streams/threaded_worker.h"

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
struct file_cbor_dump_observer : streams::observer<cbor_item_t*> {
  explicit file_cbor_dump_observer(std::ostream& out) : _out(out) {}

  void on_next(cbor_item_t*&& t) override {
    CHECK_EQ(0, cbor_refcount(t));
    cbor_incref(t);
    auto t_decref = gsl::finally([&t]() { cbor_decref(&t); });
    _out << t << std::endl;
  }
  void on_error(std::error_condition ec) override {
    LOG(ERROR) << "ERROR: " << ec.message();
    delete this;
  }

  void on_complete() override { delete this; }

  std::ostream& _out;
};

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

  return cbor_move(json_to_cbor(d));
}

cbor_item_t* read_config_from_arg(const std::string& arg) {
  rapidjson::Document d;
  rapidjson::ParseResult ok = d.Parse(arg.c_str());

  if (!ok) {
    std::cerr << "Config parse error at offset " << ok.Offset() << ": "
              << GetParseError_En(ok.Code()) << std::endl;
    exit(1);
  }

  return cbor_move(json_to_cbor(d));
}

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
  CHECK_EQ(0, cbor_refcount(msg.data));
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

int bot_environment::main(int argc, char* argv[]) {
  cli_streams::configuration cli_cfg;
  cli_cfg.enable_rtm_input = true;
  cli_cfg.enable_file_input = true;
  cli_cfg.enable_camera_input = true;
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
      id, batch ? execution_mode::BATCH : execution_mode::LIVE, _bot_descriptor);

  cbor_item_t* bot_config{nullptr};
  if (cmd_args.count("config-file") > 0) {
    bot_config = read_config_from_file(cmd_args["config-file"].as<std::string>());
  } else if (cmd_args.count("config") > 0) {
    bot_config = read_config_from_arg(cmd_args["config"].as<std::string>());
  }

  _bot_instance->configure(bot_config);

  boost::asio::ssl::context ssl_context{boost::asio::ssl::context::sslv23};

  _rtm_client = cli_cfg.rtm_client(cmd_args, io_service, std::this_thread::get_id(),
                                   ssl_context, *this);
  if (_rtm_client) {
    if (auto ec = _rtm_client->start()) {
      ABORT() << "error starting rtm client: " << ec.message();
    }
  }

  const std::string channel = cli_cfg.rtm_channel(cmd_args);
  const bool batch_mode = cli_cfg.is_batch_mode(cmd_args);

  auto single_frame_source = cli_cfg.decoded_publisher(
      cmd_args, io_service, _rtm_client, channel, _bot_descriptor.pixel_format);
  if (!batch_mode) {
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

  if (cmd_args.count("analysis-file") > 0) {
    std::string analysis_file = cmd_args["analysis-file"].as<std::string>();
    LOG(INFO) << "saving analysis output to " << analysis_file;
    _analysis_file = std::make_unique<std::ofstream>(analysis_file.c_str());
    _analysis_sink = new file_cbor_dump_observer(*_analysis_file);
  } else if (_rtm_client) {
    _analysis_sink =
        &rtm::cbor_sink(_rtm_client, io_service, channel + analysis_channel_suffix);
  } else {
    _analysis_sink = new file_cbor_dump_observer(std::cout);
  }

  if (cmd_args.count("debug-file") > 0) {
    std::string debug_file = cmd_args["debug-file"].as<std::string>();
    LOG(INFO) << "saving debug output to " << debug_file;
    _debug_file = std::make_unique<std::ofstream>(debug_file.c_str());
    _debug_sink = new file_cbor_dump_observer(*_debug_file);
  } else if (_rtm_client) {
    _debug_sink =
        &rtm::cbor_sink(_rtm_client, io_service, channel + debug_channel_suffix);
  } else {
    _debug_sink = new file_cbor_dump_observer(std::cerr);
  }

  if (_rtm_client) {
    _control_sink = &rtm::cbor_sink(_rtm_client, io_service, control_channel);
    _control_source = rtm::cbor_channel(_rtm_client, control_channel, {});
  } else {
    _control_sink = new file_cbor_dump_observer(std::cout);
    _control_source = streams::publishers::empty<cbor_item_t*>();
  }

  bool finished{false};
  int frames_count = 0;

  _source = std::move(_source) >> streams::signal_breaker<std::queue<owned_image_packet>>(
                                      {SIGINT, SIGTERM, SIGQUIT})
            >> streams::map([&frames_count](std::queue<owned_image_packet>&& pkt) {
                frames_count++;
                constexpr int period = 100;
                if ((frames_count % period) == 0) {
                  LOG(INFO) << "Processed " << frames_count << " multiframes";
                }
                return pkt;
              })
            >> streams::do_finally([this, &finished, &io_service]() {
                finished = true;

                io_service.post([this]() {
                  if (_rtm_client) {
                    if (auto ec = _rtm_client->stop()) {
                      LOG(ERROR) << "error stopping rtm client: " << ec.message();
                    } else {
                      LOG(INFO) << "rtm client was stopped";
                    }
                  }
                });
              });

  auto bot_input_stream = streams::publishers::merge<bot_input>(
      std::move(_control_source)
          >> streams::map([](cbor_item_t*&& t) { return bot_input{t}; }),
      std::move(_source) >> streams::map([](std::queue<owned_image_packet>&& p) {
        return bot_input{p};
      }));

  auto bot_output_stream = std::move(bot_input_stream) >> _bot_instance->run_bot();

  bot_output_stream->process([this](bot_output&& o) { boost::apply_visitor(*this, o); });

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
