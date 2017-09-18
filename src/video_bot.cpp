#include <execinfo.h>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/bind.hpp>
#include <boost/program_options.hpp>
#include <boost/scope_exit.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <list>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "base64.h"
#include "bot_environment.h"
#include "cbor_json.h"
#include "librtmvideo/cbor_tools.h"
#include "librtmvideo/data.h"
#include "librtmvideo/decoder.h"
#include "librtmvideo/rtmvideo.h"
#include "librtmvideo/tele.h"
#include "librtmvideo/video_bot.h"
#include "logging_implementation.h"
#include "rtmclient.h"
#include "stopwatch.h"
#include "tele_impl.h"
#include "video_streams.h"
#include "worker.h"

namespace asio = boost::asio;

namespace rtm {
namespace video {

namespace {

constexpr size_t network_frames_max_buffer_size = 1024;
constexpr size_t encoded_frames_max_buffer_size = 32;
constexpr size_t image_frames_max_buffer_size = 2;

auto frames_received = tele::counter_new("vbot", "frames_received");
auto messages_received = tele::counter_new("vbot", "messages_received");
auto bytes_received = tele::counter_new("vbot", "bytes_received");
auto metadata_received = tele::counter_new("vbot", "metadata_received");

auto encoded_frame_buffer_size = tele::gauge_new("vbot", "encoded_frame_buffer_size");
auto image_frame_buffer_size = tele::gauge_new("vbot", "image_frame_buffer_size");

auto decoding_times_millis = tele::distribution_new("vbot", "decoding_times_millis");
auto processing_times_millis = tele::distribution_new("vbot", "processing_times_millis");

auto encoded_frames_dropped = tele::counter_new("vbot", "encoded_frames_dropped");
auto image_frames_dropped = tele::counter_new("vbot", "image_frames_dropped");

struct bot_message {
  cbor_item_t* data;
  bot_message_kind kind;
  frame_id id;
};

struct channel_names {
  explicit channel_names(const std::string& base_name)
      : control(control_channel),
        analysis(base_name + analysis_channel_suffix),
        debug(base_name + debug_channel_suffix) {}

  const std::string control;
  const std::string analysis;
  const std::string debug;
};

void sigsegv_handler(int sig) {
  constexpr size_t max_backtrace_depth = 50;
  void* array[max_backtrace_depth];

  // get void*'s for all entries on the stack
  auto size = backtrace(array, max_backtrace_depth);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

}  // namespace

class bot_api_exception : public std::runtime_error {
 public:
  bot_api_exception() : runtime_error("bot api error") {}
};

encoded_metadata decode_metadata_frame(const rapidjson::Value& msg) {
  std::string codec_data =
      msg.HasMember("codecData") ? decode64(msg["codecData"].GetString()) : "";
  std::string codec_name = msg["codecName"].GetString();
  return {codec_name, codec_data};
}

class bot_instance : public bot_context, public streams::subscriber<owned_image_frame> {
 public:
  bot_instance(const std::string& bot_id, const bot_descriptor& descriptor,
               rtm::video::bot_environment& env)
      : _bot_id(bot_id), _descriptor(descriptor), _env(env) {
    frame_metadata = &_image_metadata;
  }

  virtual void stop() {
    if (_sub) {
      _sub->cancel();
    }
  }

  void queue_message(const bot_message_kind kind, cbor_item_t* message,
                     const frame_id& id) {
    bot_message newmsg{message, kind, id};
    cbor_incref(message);
    _message_buffer.push_back(newmsg);
  }

  void get_metadata(image_metadata& data) const {
    data.width = _image_metadata.width;
    data.height = _image_metadata.height;
    std::copy(_image_metadata.plane_strides,
              _image_metadata.plane_strides + MAX_IMAGE_PLANES, data.plane_strides);
  }

  streams::subscription* _sub{nullptr};

  void on_subscribe(streams::subscription& s) override {
    _sub = &s;
    _sub->request(1);
  }

  void on_complete() override {
    LOG_S(INFO) << "processing complete\n";
    _sub = nullptr;
  }

  void on_error(std::error_condition ec) override {
    LOG_S(ERROR) << ec.message() << "\n";
    _sub = nullptr;
    exit(2);
  }

  void on_next(owned_image_frame&& f) override {
    process_image_frame(std::move(f));
    _sub->request(1);
  }

 protected:
  virtual void process_image_frame(owned_image_frame&& frame) {
    stopwatch<> s;

    if (frame.width != _image_metadata.width) {
      _image_metadata.width = frame.width;
      _image_metadata.height = frame.height;
      std::copy(frame.plane_strides, frame.plane_strides + MAX_IMAGE_PLANES,
                _image_metadata.plane_strides);
    }
    image_frame bframe{.id = frame.id};

    for (int i = 0; i < MAX_IMAGE_PLANES; ++i) {
      if (frame.plane_data[i].empty()) {
        bframe.plane_data[i] = nullptr;
      } else {
        bframe.plane_data[i] = (const uint8_t*)frame.plane_data[i].data();
      }
    }

    _descriptor.img_callback(*this, bframe);
    tele::distribution_add(processing_times_millis, s.millis());

    send_messages(frame.id);
  }

  virtual void transmit(const bot_message_kind kind, cbor_item_t* message) = 0;

  void send_messages(const frame_id& id) {
    for (auto&& msg : _message_buffer) {
      cbor_item_t* data = msg.data;

      int64_t ei1 = msg.id.i1 == 0 ? id.i1 : msg.id.i1;
      int64_t ei2 = msg.id.i2 == 0 ? id.i1 : msg.id.i2;

      if (ei1 >= 0) {
        cbor_item_t* is = cbor_new_definite_array(2);
        cbor_array_set(is, 0, cbor_move(cbor_build_uint64(static_cast<uint64_t>(ei1))));
        cbor_array_set(is, 1, cbor_move(cbor_build_uint64(static_cast<uint64_t>(ei2))));
        cbor_map_add(data,
                     {.key = cbor_move(cbor_build_string("i")), .value = cbor_move(is)});
      }

      if (!_bot_id.empty()) {
        cbor_map_add(data, {.key = cbor_move(cbor_build_string("from")),
                            .value = cbor_move(cbor_build_string(_bot_id.c_str()))});
      }

      transmit(msg.kind, msg.data);
      cbor_decref(&msg.data);
    }
    _message_buffer.clear();
  }

  rtm::video::bot_environment& _env;
  std::list<rtm::video::bot_message> _message_buffer;
  std::shared_ptr<decoder> _decoder;
  std::mutex _decoder_mutex;
  encoded_metadata _metadata;
  image_metadata _image_metadata{0, 0};

  const std::string _bot_id;
  const bot_descriptor _descriptor;
};

class bot_offline_instance : public bot_instance {
 public:
  bot_offline_instance(const std::string& bot_id, const bot_descriptor& descriptor,
                       std::ostream& analysis, std::ostream& debug,
                       rtm::video::bot_environment& env)
      : bot_instance(bot_id, descriptor, env), _analysis(analysis), _debug(debug) {}

 private:
  void transmit(const bot_message_kind kind, cbor_item_t* message) override {
    switch (kind) {
      case bot_message_kind::ANALYSIS:
        cbor::dump(_analysis, message);
        _analysis << std::endl;
        break;
      default:
        cbor::dump(_debug, message);
        _debug << std::endl;
    }
  }

  std::ostream &_analysis, &_debug;
};

class bot_online_instance : public bot_instance, public rtm::subscription_callbacks {
 public:
  bot_online_instance(const std::string& bot_id, const bot_descriptor& descriptor,
                      const std::string& channel, rtm::video::bot_environment& env)
      : bot_instance(bot_id, descriptor, env), _channels(channel) {}

  void subscribe_to_control_channel(rtm::subscriber& s) {
    if (!_bot_id.empty() && _descriptor.ctrl_callback) {
      s.subscribe_channel(_channels.control, _control_subscription, *this);
    }
  }

  void on_error(std::error_condition ec) override {
    LOG_S(ERROR) << ec.message() << "\n";
    throw bot_api_exception();
  }

  void on_data(const subscription& sub, rapidjson::Value&& value) override {
    if (&sub == &_control_subscription) {
      on_control_message(std::move(value));
    } else {
      BOOST_ASSERT_MSG(false, "Unknown subscription");
    }
  }

 private:
  void transmit(const bot_message_kind kind, cbor_item_t* message) override {
    std::string channel;
    switch (kind) {
      case bot_message_kind::ANALYSIS:
        channel = _channels.analysis;
        break;
      case bot_message_kind::DEBUG:
        channel = _channels.debug;
      case bot_message_kind::CONTROL:
        channel = _channels.control;
        break;
    }

    _env.publisher().publish(channel, message);
  }

  void on_control_message(rapidjson::Value&& msg) {
    if (msg.IsArray()) {
      for (auto& m : msg.GetArray()) on_control_message(std::move(m));
      return;
    }

    if (!msg.IsObject()) {
      LOG_S(ERROR) << "unsupported kind of message\n";
      return;
    }

    if (!msg.HasMember("to") || _bot_id != msg["to"].GetString()) {
      return;
    }

    cbor_item_t* request = json_to_cbor(msg);
    auto cbor_deleter = gsl::finally([&request]() { cbor_decref(&request); });
    cbor_item_t* response = _descriptor.ctrl_callback(*this, request);

    if (response != nullptr) {
      BOOST_ASSERT(cbor_isa_map(response));
      cbor_item_t* request_id = cbor::map_get(request, "request_id");
      if (request_id != nullptr) {
        cbor_map_add(response,
                     (struct cbor_pair){.key = cbor_move(cbor_build_string("request_id")),
                                        .value = cbor_move(cbor_copy(request_id))});
      }

      queue_message(bot_message_kind::CONTROL, cbor_move(response), frame_id{0, 0});
    }

    send_messages({.i1 = -1, .i2 = -1});
  }

  const channel_names _channels;
  const rtm::subscription _control_subscription{};
};

cbor_item_t* configure_command(cbor_item_t* config) {
  cbor_item_t* cmd = cbor_new_definite_map(2);
  cbor_map_add(cmd,
               (struct cbor_pair){.key = cbor_move(cbor_build_string("action")),
                                  .value = cbor_move(cbor_build_string("configure"))});
  cbor_map_add(cmd, (struct cbor_pair){.key = cbor_move(cbor_build_string("body")),
                                       .value = config});
  return cmd;
}

bot_environment& bot_environment::instance() {
  static bot_environment env;
  return env;
}

void bot_environment::register_bot(const bot_descriptor* bot) {
  assert(!_bot_descriptor);
  _bot_descriptor = bot;
}

variables_map parse_command_line(int argc, char* argv[]) {
  namespace po = boost::program_options;
  po::options_description generic("Generic options");
  generic.add_options()("help", "produce help message")(
      "config", po::value<std::string>(), "bot config file")(
      "id", po::value<std::string>()->default_value(""), "bot id");

  po::options_description online("Online options");
  online.add_options()("endpoint", po::value<std::string>(), "app endpoint")(
      "appkey", po::value<std::string>(), "app key")("channel", po::value<std::string>(),
                                                     "channel")(
      "port", po::value<std::string>(), "port")("time_limit", po::value<double>(),
                                                "time in seconds");

  po::options_description offline("Offline options");
  offline.add_options()("video_file", po::value<std::string>(), "input mp4")(
      "replay_file", po::value<std::string>(), "input txt")(
      "analysis_file", po::value<std::string>(), "output txt")(
      "debug_file", po::value<std::string>(), "output txt")(
      "synchronous", po::bool_switch()->default_value(false), "disable drops");

  po::options_description desc;
  desc.add(generic).add(online).add(offline);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help") || argc == 1) {
    std::cout << desc << "\n";
    exit(1);
  }

  bool online_mode = (vm.count("endpoint") || vm.count("appkey") || vm.count("channel")
                      || vm.count("port") || vm.count("time_limit"));

  bool offline_mode = (vm.count("video_file") || vm.count("analysis_file")
                       || vm.count("debug_file") || vm.count("replay_file"));

  if (online_mode && offline_mode) {
    std::cerr << "Online and offline modes are mutually exclusive"
              << "\n";
    exit(1);
  }

  if (online_mode) {
    if (!vm.count("endpoint")) {
      std::cerr << "Missing --endpoint argument"
                << "\n";
      exit(1);
    }

    if (!vm.count("appkey")) {
      std::cerr << "Missing --appkey argument"
                << "\n";
      exit(1);
    }

    if (!vm.count("channel")) {
      std::cerr << "Missing --channel argument"
                << "\n";
      exit(1);
    }

    if (!vm.count("port")) {
      std::cerr << "Missing --port argument"
                << "\n";
      exit(1);
    }
  }

  if (offline_mode) {
    if (!vm.count("video_file") && !vm.count("replay_file")) {
      std::cerr << "Missing --video_file or --replay_file argument"
                << "\n";
      exit(1);
    }
    if (vm.count("video_file") && vm.count("replay_file")) {
      std::cerr << "--video_file and --replay_file are mutually exclusive"
                << "\n";
      exit(1);
    }
  }

  return vm;
}

void bot_environment::parse_config(boost::optional<std::string> config_file) {
  if (!_bot_descriptor->ctrl_callback) {
    if (config_file.is_initialized())
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

int bot_environment::main_online(variables_map cmd_args) {
  const std::string id = cmd_args["id"].as<std::string>();
  const std::string channel = cmd_args["channel"].as<std::string>();
  bot_online_instance* _bot_online_instance =
      new bot_online_instance(id, *_bot_descriptor, channel, *this);
  _bot_instance.reset(_bot_online_instance);

  parse_config(cmd_args.count("config")
                   ? boost::optional<std::string>{cmd_args["config"].as<std::string>()}
                   : boost::optional<std::string>{});

  const std::string endpoint = cmd_args["endpoint"].as<std::string>();
  const std::string appkey = cmd_args["appkey"].as<std::string>();
  const std::string port = cmd_args["port"].as<std::string>();

  decoder_init_library();
  boost::asio::io_service io_service;
  boost::asio::ssl::context ssl_context{asio::ssl::context::sslv23};

  boost::asio::signal_set signals(io_service);
  signals.add(SIGINT);
  signals.add(SIGTERM);
  signals.add(SIGQUIT);
  signals.async_wait(std::bind(&boost::asio::io_service::stop, &io_service));

  _client = std::make_shared<resilient_client>(
      [&endpoint, &port, &appkey, &io_service, &ssl_context, this]() {
        return rtm::new_client(endpoint, port, appkey, io_service, ssl_context, 1, *this);
      });
  _client->start();
  _bot_online_instance->subscribe_to_control_channel(*_client);
  _source = rtm_source(_client, channel)
            >> buffered_worker("vbot.network_buffer", network_frames_max_buffer_size)
            >> streams::lift(decode_network_stream())
            >> buffered_worker("vbot.encoded_buffer", encoded_frames_max_buffer_size)
            >> streams::lift(decode_image_frames(_bot_descriptor->image_width,
                                                 _bot_descriptor->image_height,
                                                 _bot_descriptor->pixel_format))
            >> buffered_worker("vbot.image_buffer", encoded_frames_max_buffer_size)
            >> streams::do_finally([this]() { _client->stop(); });
  _source->subscribe(*_bot_instance);

  tele::publisher tele_publisher(*_client, io_service);
  boost::asio::deadline_timer timer(io_service);

  if (cmd_args.count("time_limit")) {
    timer.expires_from_now(
        boost::posix_time::seconds(cmd_args["time_limit"].as<double>()));
    timer.async_wait([&io_service](const boost::system::error_code& ec) {
      io_service.stop();
      exit(0);
    });
  }

  io_service.run();
  _bot_instance->stop();

  return 0;
}

int bot_environment::main_offline(variables_map cmd_args) {
  const std::string id = cmd_args["id"].as<std::string>();
  rtm::video::initialize_source_library();
  std::ostream *_analysis, *_debug;
  if (cmd_args.count("analysis_file")) {
    _analysis = new std::ofstream(cmd_args["analysis_file"].as<std::string>().c_str());
  } else
    _analysis = &std::cout;
  if (cmd_args.count("debug_file")) {
    _debug = new std::ofstream(cmd_args["debug_file"].as<std::string>().c_str());
  } else
    _debug = &std::cerr;

  bot_offline_instance* _bot_offline_instance =
      new bot_offline_instance(id, *_bot_descriptor, *_analysis, *_debug, *this);
  _bot_instance.reset(_bot_offline_instance);

  parse_config(cmd_args.count("config")
                   ? boost::optional<std::string>{cmd_args["config"].as<std::string>()}
                   : boost::optional<std::string>{});

  streams::publisher<encoded_packet> src;

  boost::asio::io_service io_service;

  if (cmd_args.count("video_file"))
    src = file_source(io_service, cmd_args["video_file"].as<std::string>(), false,
                      cmd_args["synchronous"].as<bool>());
  else {
    src = network_replay_source(io_service, cmd_args["replay_file"].as<std::string>(),
                                cmd_args["synchronous"].as<bool>())
          >> streams::lift(decode_network_stream());
  }

  _source =
      std::move(src) >> streams::lift(decode_image_frames(_bot_descriptor->image_width,
                                                          _bot_descriptor->image_height,
                                                          _bot_descriptor->pixel_format));
  _source->subscribe(*_bot_instance);

  io_service.run();

  _analysis->flush();
  _debug->flush();
  _bot_instance->stop();

  return 0;
}  // namespace video

int bot_environment::main(int argc, char* argv[]) {
  loguru::init(argc, argv);
  loguru::g_stderr_verbosity = 1;
  signal(SIGSEGV, sigsegv_handler);

  auto cmd_args = parse_command_line(argc, argv);

  if (cmd_args.count("channel")) {  // check for online mode
    return main_online(cmd_args);
  } else {  // offline mode
    return main_offline(cmd_args);
  }
}

}  // namespace video
}  // namespace rtm

void rtm_video_bot_message(bot_context& ctx, const bot_message_kind kind,
                           cbor_item_t* message, const frame_id& id) {
  BOOST_ASSERT_MSG(cbor_map_is_indefinite(message), "Message must be indefinite map");
  static_cast<rtm::video::bot_instance&>(ctx).queue_message(kind, message, id);
}

void rtm_video_bot_get_metadata(image_metadata& data, const bot_context& ctx) {
  static_cast<const rtm::video::bot_instance&>(ctx).get_metadata(data);
}

void rtm_video_bot_register(const bot_descriptor& bot) {
  rtm::video::bot_environment::instance().register_bot(&bot);
}

int rtm_video_bot_main(int argc, char* argv[]) {
  return rtm::video::bot_environment::instance().main(argc, argv);
}
