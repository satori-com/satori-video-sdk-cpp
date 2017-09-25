#include "bot_environment.h"

#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <fstream>

#include "asio_streams.h"
#include "avutils.h"
#include "bot_instance.h"
#include "cbor_json.h"
#include "cli_streams.h"
#include "librtmvideo/cbor_tools.h"
#include "logging_implementation.h"
#include "rtm_streams.h"
#include "signal_breaker.h"
#include "worker.h"

namespace rtm {
namespace video {
namespace {

constexpr size_t network_frames_max_buffer_size = 1024;
constexpr size_t encoded_frames_max_buffer_size = 32;
constexpr size_t image_frames_max_buffer_size = 2;

variables_map parse_command_line(int argc, char* argv[],
                                 const cli_streams::configuration& cli_cfg) {
  namespace po = boost::program_options;

  po::options_description generic("Generic options");
  generic.add_options()("help", "produce help message");
  generic.add_options()(",v", po::value<std::string>(),
                        "log verbosity level (INFO, WARNING, ERROR, FATAL, OFF, 1-9)");

  po::options_description bot_configuration_options("Bot configuration options");
  bot_configuration_options.add_options()(
      "id", po::value<std::string>()->default_value(""), "bot id");
  bot_configuration_options.add_options()("config", po::value<std::string>(),
                                          "bot config file");

  po::options_description bot_execution_options("Bot execution options");
  bot_execution_options.add_options()(
      "analysis_file", po::value<std::string>(),
      "saves analysis messages to a file instead of sending to a channel");
  bot_execution_options.add_options()(
      "debug_file", po::value<std::string>(),
      "saves debug messages to a file instead of sending to a channel");
  bot_execution_options.add_options()(
      "time_limit", po::value<long>(),
      "(seconds) if specified, bot will exit after given time elapsed");
  bot_execution_options.add_options()(
      "frames_limit", po::value<long>(),
      "(number) if specified, bot will exit after processing given number of frames");

  po::options_description cli_options = cli_cfg.to_boost();
  cli_options.add(bot_configuration_options).add(bot_execution_options).add(generic);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, cli_options), vm);
  po::notify(vm);

  if (argc == 1 || vm.count("help")) {
    std::cerr << cli_options << "\n";
    exit(1);
  }

  if (!cli_cfg.validate(vm)) exit(1);

  return vm;
}

// todo: move this reusable class out.
struct file_cbor_dump_observer : public streams::observer<cbor_item_t*> {
  explicit file_cbor_dump_observer(std::ostream& out) : _out(out) {}

  void on_next(cbor_item_t*&& t) override {
    cbor::dump(_out, t);
    _out << std::endl;
    cbor_decref(&t);
  }
  void on_error(std::error_condition ec) override {
    LOG_S(ERROR) << "ERROR: " << ec.message();
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

void log_important_counters() {
  LOG_S(INFO) << "  input.network_buffer.delivered = " << std::setw(5) << std::left
              << tele::counter_get("input.network_buffer.delivered")
              << "  input.network_buffer.dropped = " << std::setw(5) << std::left
              << tele::counter_get("input.network_buffer.dropped")
              << "  input.network_buffer.size = " << std::setw(2) << std::left
              << tele::gauge_get("input.network_buffer.size");

  LOG_S(INFO) << "  input.encoded_buffer.delivered = " << std::setw(5) << std::left
              << tele::counter_get("input.encoded_buffer.delivered")
              << "  input.encoded_buffer.dropped = " << std::setw(5) << std::left
              << tele::counter_get("input.encoded_buffer.dropped")
              << "  input.encoded_buffer.size = " << std::setw(2) << std::left
              << tele::gauge_get("input.encoded_buffer.size");

  LOG_S(INFO) << "    input.image_buffer.delivered = " << std::setw(5) << std::left
              << tele::counter_get("input.image_buffer.delivered")
              << "    input.image_buffer.dropped = " << std::setw(5) << std::left
              << tele::counter_get("input.image_buffer.dropped")
              << "    input.image_buffer.size = " << std::setw(2) << std::left
              << tele::gauge_get("input.image_buffer.size");
}

}  // namespace

void bot_environment::parse_config(boost::optional<std::string> config_file) {
  if (!_bot_descriptor->ctrl_callback && config_file.is_initialized()) {
    std::cerr << "Config specified but there is no control method set\n";
    exit(1);
  }

  cbor_item_t* config;

  if (config_file.is_initialized()) {
    FILE* fp = fopen(config_file.get().c_str(), "r");
    if (!fp) {
      std::cerr << "Can't read config file " << config_file.get() << ": "
                << strerror(errno) << "\n";
      exit(1);
    }
    auto file_closer = gsl::finally([&fp]() { fclose(fp); });

    char readBuffer[65536];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    rapidjson::Document d;
    d.ParseStream(is);

    config = json_to_cbor(d);
  } else {
    config = cbor_new_definite_map(0);
  }
  cbor_item_t* cmd = configure_command(config);
  auto cbor_deleter = gsl::finally([&config, &cmd]() {
    cbor_decref(&config);
    cbor_decref(&cmd);
  });

  cbor_item_t* response = _bot_descriptor->ctrl_callback(*_bot_instance, cmd);
  if (response != nullptr) {
    _bot_instance->queue_message(bot_message_kind::DEBUG, cbor_move(response),
                                 frame_id{0, 0});
  }
}

bot_environment& bot_environment::instance() {
  static bot_environment env;
  return env;
}

void bot_environment::register_bot(const bot_descriptor* bot) {
  assert(!_bot_descriptor);
  _bot_descriptor = bot;
}

void bot_environment::send_messages(std::list<bot_message>&& messages) {
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
  cli_cfg.enable_file_batch_mode = true;

  auto cmd_args = parse_command_line(argc, argv, cli_cfg);
  init_logging(argc, argv);

  const std::string id = cmd_args["id"].as<std::string>();
  _bot_instance.reset(new bot_instance(id, *_bot_descriptor, *this));
  parse_config(cmd_args.count("config")
                   ? boost::optional<std::string>{cmd_args["config"].as<std::string>()}
                   : boost::optional<std::string>{});

  boost::asio::io_service io_service;
  boost::asio::ssl::context ssl_context{boost::asio::ssl::context::sslv23};

  _rtm_client = cli_cfg.rtm_client(cmd_args, io_service, ssl_context, *this);
  if (_rtm_client) {
    _rtm_client->start();
    _tele_publisher.reset(new tele::publisher(*_rtm_client, io_service));
  }

  const std::string channel = cli_cfg.rtm_channel(cmd_args);

  streams::publisher<encoded_packet> encoded_src =
      cli_cfg.encoded_publisher(cmd_args, io_service, _rtm_client, channel, true);

  bool batch_mode = cli_cfg.is_batch_mode(cmd_args);
  auto decode_op =
      decode_image_frames(_bot_descriptor->image_width, _bot_descriptor->image_height,
                          _bot_descriptor->pixel_format);

  _source = std::move(encoded_src) >> std::move(decode_op);
  if (!batch_mode) {
    _source = std::move(_source)
              >> buffered_worker("input.image_buffer", image_frames_max_buffer_size);
  }

  if (cmd_args.count("analysis_file")) {
    std::string analysis_file = cmd_args["analysis_file"].as<std::string>();
    LOG_S(INFO) << "saving analysis output to " << analysis_file;
    _analysis_file.reset(new std::ofstream(analysis_file.c_str()));
    _analysis_sink = new file_cbor_dump_observer(*_analysis_file);
  } else if (_rtm_client) {
    _analysis_sink =
        &streams::rtm::cbor_sink(_rtm_client, channel + analysis_channel_suffix);
  } else {
    _analysis_sink = new file_cbor_dump_observer(std::cout);
  }

  if (cmd_args.count("debug_file")) {
    std::string debug_file = cmd_args["debug_file"].as<std::string>();
    LOG_S(INFO) << "saving debug output to " << debug_file;
    _debug_file.reset(new std::ofstream(debug_file.c_str()));
    _debug_sink = new file_cbor_dump_observer(*_debug_file);
  } else if (_rtm_client) {
    _debug_sink = &streams::rtm::cbor_sink(_rtm_client, channel + debug_channel_suffix);
  } else {
    _debug_sink = new file_cbor_dump_observer(std::cerr);
  }

  if (_rtm_client) {
    _control_sink = &streams::rtm::cbor_sink(_rtm_client, control_channel);
    _control_source = streams::rtm::cbor_channel(_rtm_client, control_channel, {});
  } else {
    _control_sink = new file_cbor_dump_observer(std::cout);
    _control_source = streams::publishers::empty<cbor_item_t*>();
  }

  if (cmd_args.count("time_limit")) {
    _source = std::move(_source)
              >> streams::asio::timer_breaker<owned_image_packet>(
                     io_service, std::chrono::seconds(cmd_args["time_limit"].as<long>()));
  }

  if (cmd_args.count("frames_limit")) {
    _source = std::move(_source) >> streams::take(cmd_args["frames_limit"].as<long>());
  }

  bool finished{false};
  int frames_count = 0;

  _source = std::move(_source)
            >> streams::signal_breaker<owned_image_packet>({SIGINT, SIGTERM, SIGQUIT})
            >> streams::map([frames_count](owned_image_packet&& pkt) mutable {
                frames_count++;
                constexpr int period = 100;
                if (!(frames_count % period)) {
                  LOG_S(INFO) << "Processed " << period << " frames";
                  log_important_counters();
                }
                return pkt;
              })
            >> streams::do_finally([this, &finished]() {
                finished = true;
                _bot_instance->stop();
                _tele_publisher.reset();
                if (_rtm_client) {
                  _rtm_client->stop();
                }
              });

  _bot_instance->start(_source, _control_source);

  if (!batch_mode) {
    LOG_S(INFO) << "entering asio loop";
    auto n = io_service.run();
    LOG_S(INFO) << "asio loop exited, executed " << n << " handlers";

    while (!finished) {
      // batch mode has no threads
      LOG_S(INFO) << "waiting for all threads to finish...";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  return 0;
}

}  // namespace video
}  // namespace rtm
