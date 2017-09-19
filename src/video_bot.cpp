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
#include "rtm_streams.h"
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

auto decoding_times_millis = tele::distribution_new("vbot", "decoding_times_millis");
auto processing_times_millis = tele::distribution_new("vbot", "processing_times_millis");

struct channel_names {
  explicit channel_names(const std::string& base_name)
      : control(control_channel),
        analysis(base_name + analysis_channel_suffix),
        debug(base_name + debug_channel_suffix) {}

  const std::string control;
  const std::string analysis;
  const std::string debug;
};

}  // namespace

class bot_instance : public bot_context, streams::subscriber<owned_image_packet> {
 public:
  bot_instance(const std::string& bot_id, const bot_descriptor& descriptor,
               rtm::video::bot_environment& env)
      : _bot_id(bot_id), _descriptor(descriptor), _env(env) {
    frame_metadata = &_image_metadata;
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

  struct control_sub : public streams::subscriber<cbor_item_t*> {
    explicit control_sub(bot_instance* const bot) : _bot(bot) {}

    ~control_sub() {
      if (_sub) _sub->cancel();
    }

    void on_next(cbor_item_t*&& t) override {
      _bot->process_control_message(t);
      _sub->request(1);
    }

    void on_error(std::error_condition ec) override {
      LOG_S(ERROR) << "Error in control stream: " << ec.message();
      _sub = nullptr;
      exit(1);
    }

    void on_complete() override { _sub = nullptr; }

    void on_subscribe(streams::subscription& s) override {
      _sub = &s;
      _sub->request(1);
    }

    bot_instance* const _bot;
    streams::subscription* _sub{nullptr};
  };
  std::unique_ptr<control_sub> _control_sub;

  void start(streams::publisher<owned_image_packet>& video_stream,
             streams::publisher<cbor_item_t*>& control_stream) {
    _control_sub.reset(new control_sub(this));
    video_stream->subscribe(*this);
    control_stream->subscribe(*_control_sub);
  }

  void stop() {
    if (_sub) {
      _sub->cancel();
      _sub = nullptr;
    }
    _control_sub.reset();
  }

 private:
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

  void on_next(owned_image_packet&& packet) override {
    if (const owned_image_metadata* m = boost::get<owned_image_metadata>(&packet)) {
      process_image_metadata(*m);
    } else if (const owned_image_frame* f = boost::get<owned_image_frame>(&packet)) {
      process_image_frame(*f);
    } else {
      BOOST_ASSERT_MSG(false, "Bad variant");
    }
    _sub->request(1);
  }

  virtual void process_image_metadata(const owned_image_metadata& metadata) {}

  virtual void process_image_frame(const owned_image_frame& frame) {
    LOG_S(1) << "process_image_frame " << frame.width << "x" << frame.height;
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
    }
    _env.send_messages(std::move(_message_buffer));
  }

  void process_control_message(cbor_item_t* msg) {
    auto cbor_deleter = gsl::finally([&msg]() { cbor_decref(&msg); });

    if (cbor_isa_array(msg)) {
      for (int i = 0; i < cbor_array_size(msg); ++i) {
        process_control_message(cbor_array_get(msg, i));
      }
      return;
    }

    if (!cbor_isa_map(msg)) {
      LOG_S(ERROR) << "unsupported kind of message\n";
      return;
    }

    if (_bot_id.empty() || cbor::map_get_str(msg, "to", "") != _bot_id) {
      return;
    }

    cbor_item_t* response = _descriptor.ctrl_callback(*this, msg);

    if (response != nullptr) {
      BOOST_ASSERT(cbor_isa_map(response));
      cbor_item_t* request_id = cbor::map_get(msg, "request_id");
      if (request_id != nullptr) {
        cbor_map_add(response,
                     (struct cbor_pair){.key = cbor_move(cbor_build_string("request_id")),
                                        .value = cbor_move(cbor_copy(request_id))});
      }

      queue_message(bot_message_kind::CONTROL, cbor_move(response), frame_id{0, 0});
    }

    cbor_decref(&msg);
    send_messages({.i1 = -1, .i2 = -1});
  }

  rtm::video::bot_environment& _env;
  std::list<rtm::video::bot_message> _message_buffer;
  image_metadata _image_metadata{0, 0};

  const std::string _bot_id;
  const bot_descriptor _descriptor;
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
  generic.add_options()("help", "produce help message");
  generic.add_options()(",v", po::value<std::string>(),
                        "log verbosity level (INFO, WARNING, ERROR, FATAL, OFF, 1-9)");

  po::options_description bot_config("Bot configuration options");
  bot_config.add_options()("id", po::value<std::string>()->default_value(""), "bot id")(
      "config", po::value<std::string>(), "bot config file");

  po::options_description online("Satori video source");
  online.add_options()("endpoint", po::value<std::string>(), "app endpoint")(
      "appkey", po::value<std::string>(), "app key")(
      "port", po::value<std::string>(), "port")("channel", po::value<std::string>(),
                                                "channel");

  po::options_description file_sources("File sources");
  file_sources.add_options()("video_file", po::value<std::string>(), "input mp4")(
      "replay_file", po::value<std::string>(), "input txt");

  po::options_description execution("Execution options");
  execution.add_options()(
      "batch", po::bool_switch()->default_value(false),
      "turns on batch analysis mode, where analysis of a single video frame might take "
      "longer than frame duration (file source only).")(
      "analysis_file", po::value<std::string>(),
      "saves analysis messages to a file instead of sending to a channel")(
      "debug_file", po::value<std::string>(),
      "saves debug messages to a file instead of sending to a channel")(
      "time_limit", po::value<long>(),
      "(seconds) if specified, bot will exit after given time elapsed")(
      "frames_limit", po::value<long>(),
      "(number) if specified, bot will exit after processing given number of frames");

  po::options_description desc;
  desc.add(bot_config).add(online).add(file_sources).add(execution).add(generic);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help") || argc == 1) {
    std::cerr << desc << "\n";
    exit(1);
  }

  bool satori_video_source = (vm.count("endpoint") || vm.count("appkey")
                              || vm.count("channel") || vm.count("port"));

  bool file_source = (vm.count("video_file") || vm.count("replay_file"));

  if (satori_video_source && file_source) {
    std::cerr << "Only one video source has should be specified"
              << "\n";
    exit(1);
  }

  if (satori_video_source) {
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

  if (file_source) {
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
struct file_cbor_observer : public streams::observer<cbor_item_t*> {
  explicit file_cbor_observer(std::ostream& out) : _out(out) {}

  void on_next(cbor_item_t*&& t) override {
    cbor::dump(_out, t);
    _out << "\n";
    _out.flush();
    cbor_decref(&t);
  }
  void on_error(std::error_condition ec) override {
    LOG_S(ERROR) << "ERROR: " << ec.message();
    delete this;
  }

  void on_complete() override { delete this; }

  std::ostream& _out;
};

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
  auto cmd_args = parse_command_line(argc, argv);
  init_logging(argc, argv);

  decoder_init_library();
  rtm::video::initialize_source_library();

  const std::string id = cmd_args["id"].as<std::string>();
  _bot_instance.reset(new bot_instance(id, *_bot_descriptor, *this));
  parse_config(cmd_args.count("config")
                   ? boost::optional<std::string>{cmd_args["config"].as<std::string>()}
                   : boost::optional<std::string>{});

  boost::asio::io_service io_service;
  boost::asio::ssl::context ssl_context{asio::ssl::context::sslv23};

  boost::asio::signal_set signals(io_service);
  signals.add(SIGINT);
  signals.add(SIGTERM);
  signals.add(SIGQUIT);
  signals.async_wait([&io_service, this](const boost::system::error_code& error,
                                         int signal) { stop(io_service); });

  if (cmd_args.count("endpoint")) {
    const std::string endpoint = cmd_args["endpoint"].as<std::string>();
    const std::string appkey = cmd_args["appkey"].as<std::string>();
    const std::string port = cmd_args["port"].as<std::string>();

    _rtm_client = std::make_shared<resilient_client>([&endpoint, &port, &appkey,
                                                      &io_service, &ssl_context, this]() {
      return rtm::new_client(endpoint, port, appkey, io_service, ssl_context, 1, *this);
    });
    _rtm_client->start();
  }

  if (_rtm_client) {
    _tele_publisher.reset(new tele::publisher(*_rtm_client, io_service));
  }

  streams::publisher<encoded_packet> encoded_src;

  const bool batch = cmd_args["batch"].as<bool>();
  const std::string channel =
      cmd_args.count("channel") ? cmd_args["channel"].as<std::string>() : "";

  if (_rtm_client) {
    encoded_src =
        rtm_source(_rtm_client, channel)
        >> buffered_worker("vbot.network_buffer", network_frames_max_buffer_size)
        >> streams::lift(decode_network_stream());
  } else {
    if (cmd_args.count("video_file")) {
      const auto& video_file = cmd_args["video_file"].as<std::string>();
      encoded_src = file_source(io_service, video_file, false, batch);
    } else if (cmd_args.count("replay_file")) {
      const auto& replay_file = cmd_args["replay_file"].as<std::string>();
      encoded_src = network_replay_source(io_service, replay_file, batch)
                    >> streams::lift(decode_network_stream());
    }
  }

  auto decode_op =
      decode_image_frames(_bot_descriptor->image_width, _bot_descriptor->image_height,
                          _bot_descriptor->pixel_format);
  if (batch) {
    _source = std::move(encoded_src) >> streams::lift(decode_op);
  } else {
    _source = std::move(encoded_src)
              >> buffered_worker("vbot.encoded_buffer", encoded_frames_max_buffer_size)
              >> streams::lift(decode_op)
              >> buffered_worker("vbot.image_buffer", image_frames_max_buffer_size);
  }

  if (cmd_args.count("analysis_file")) {
    std::string analysis_file = cmd_args["analysis_file"].as<std::string>();
    LOG_S(INFO) << "saving analysis output to " << analysis_file;
    _analysis_file.reset(new std::ofstream(analysis_file.c_str()));
    _analysis_sink = new file_cbor_observer(*_analysis_file);
  } else if (_rtm_client) {
    _analysis_sink =
        &streams::rtm::cbor_sink(_rtm_client, channel + analysis_channel_suffix);
  } else {
    _analysis_sink = new file_cbor_observer(std::cout);
  }

  if (cmd_args.count("debug_file")) {
    std::string debug_file = cmd_args["debug_file"].as<std::string>();
    LOG_S(INFO) << "saving debug output to " << debug_file;
    _debug_file.reset(new std::ofstream(debug_file.c_str()));
    _debug_sink = new file_cbor_observer(*_debug_file);
  } else if (_rtm_client) {
    _debug_sink =
        &streams::rtm::cbor_sink(_rtm_client, channel + debug_channel_suffix);
  } else {
    _debug_sink = new file_cbor_observer(std::cerr);
  }

  if (_rtm_client) {
    _control_sink = &streams::rtm::cbor_sink(_rtm_client, control_channel);
    _control_source = streams::rtm::cbor_channel(_rtm_client, control_channel, {});
  } else {
    _control_sink = new file_cbor_observer(std::cout);
    _control_source = streams::publishers::empty<cbor_item_t*>();
  }

  boost::asio::deadline_timer timer(io_service);
  if (cmd_args.count("time_limit")) {
    timer.expires_from_now(boost::posix_time::seconds(cmd_args["time_limit"].as<long>()));
    timer.async_wait(
        [&io_service, this](const boost::system::error_code& ec) { stop(io_service); });
  }

  if (cmd_args.count("frames_limit")) {
    _source = std::move(_source) >> streams::take(cmd_args["frames_limit"].as<long>());
  }

  _source = std::move(_source)
            >> streams::do_finally([&io_service, this]() { stop(io_service); });

  _bot_instance->start(_source, _control_source);

  if (!batch) {
    LOG_S(INFO) << "entering asio loop";
    auto n = io_service.run();
    LOG_S(INFO) << "asio loop exited, executed " << n << " handlers";
  }

  return 0;
}

void bot_environment::stop(boost::asio::io_service& io) {
  _bot_instance->stop();
  _tele_publisher.reset();
  if (_rtm_client) {
    _rtm_client->stop();
  }

  exit(0);
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
