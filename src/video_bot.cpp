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
#include "flow_rtm_aggregator.h"
#include "librtmvideo/cbor_tools.h"
#include "librtmvideo/data.h"
#include "librtmvideo/decoder.h"
#include "librtmvideo/rtmpacket.h"
#include "librtmvideo/rtmvideo.h"
#include "librtmvideo/tele.h"
#include "librtmvideo/video_bot.h"
#include "rtmclient.h"
#include "sink.h"
#include "source_file.h"
#include "source_replay.h"
#include "source_rtm.h"
#include "stopwatch.h"
#include "tele_impl.h"
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

auto encoded_frame_buffer_size =
    tele::gauge_new("vbot", "encoded_frame_buffer_size");
auto image_frame_buffer_size =
    tele::gauge_new("vbot", "image_frame_buffer_size");

auto decoding_times_millis =
    tele::distribution_new("vbot", "decoding_times_millis");
auto processing_times_millis =
    tele::distribution_new("vbot", "processing_times_millis");

auto encoded_frames_dropped =
    tele::counter_new("vbot", "encoded_frames_dropped");
auto image_frames_dropped = tele::counter_new("vbot", "image_frames_dropped");

struct bot_message {
  cbor_item_t* data;
  bot_message_kind kind;
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

metadata decode_metadata_frame(const rapidjson::Value& msg) {
  std::string codec_data =
      msg.HasMember("codecData") ? decode64(msg["codecData"].GetString()) : "";
  std::string codec_name = msg["codecName"].GetString();
  return {codec_name, codec_data};
}

class bot_instance : public bot_context, public sink<metadata, encoded_frame> {
 public:
  bot_instance(const std::string& bot_id, const bot_descriptor& descriptor,
               rtm::video::bot_environment& env)
      : _bot_id(bot_id), _descriptor(descriptor), _env(env) {
    _encoded_frames_worker = std::make_unique<threaded_worker<encoded_frame>>(
        encoded_frames_max_buffer_size,
        [this](encoded_frame&& f) { process_encoded_frame(std::move(f)); });
    _image_frames_worker = std::make_unique<threaded_worker<image_frame>>(
        image_frames_max_buffer_size,
        [this](image_frame&& frame) { process_image_frame(std::move(frame)); });
  }

  void queue_message(const bot_message_kind kind, cbor_item_t* message) {
    bot_message newmsg{message, kind};
    cbor_incref(message);
    _message_buffer.push_back(newmsg);
  }

  void on_metadata(metadata&& m) override {
    tele::counter_inc(metadata_received);

    if (m.codec_data == _metadata.codec_data &&
        m.codec_name == _metadata.codec_name)
      return;

    _metadata = m;
    std::lock_guard<std::mutex> guard(_decoder_mutex);

    _decoder.reset(decoder_new_keep_proportions(_descriptor.image_width,
                                                _descriptor.image_height,
                                                _descriptor.pixel_format),
                   [this](decoder* d) {
                     std::cerr << "Deleting decoder\n";
                     decoder_delete(d);
                   });
    BOOST_VERIFY(_decoder);

    decoder_set_metadata(_decoder.get(), _metadata.codec_name.c_str(),
                         (const uint8_t*)_metadata.codec_data.data(),
                         _metadata.codec_data.size());
    std::cout << "Video decoder initialized\n";
  }

  void on_frame(encoded_frame&& f) override {
    tele::counter_inc(messages_received);
    tele::counter_inc(bytes_received, f.data.size());

    if (!_encoded_frames_worker->try_send(std::move(f))) {
      tele::counter_inc(encoded_frames_dropped);
    }

    tele::gauge_set(encoded_frame_buffer_size,
                    _encoded_frames_worker->queue_size());
    tele::gauge_set(image_frame_buffer_size,
                    _image_frames_worker->queue_size());
  }

  bool empty() override { return _encoded_frames_worker->queue_size() == 0; }

 private:
  void process_encoded_frame(encoded_frame&& f) {
    std::lock_guard<std::mutex> guard(_decoder_mutex);

    stopwatch<> s;
    decoder* decoder = _decoder.get();
    decoder_process_binary_message(decoder, (const uint8_t*)f.data.data(),
                                   f.data.size());
    tele::distribution_add(decoding_times_millis, s.millis());

    if (decoder_frame_ready(decoder)) {
      tele::counter_inc(frames_received);
      if (_descriptor.img_callback) {
        int height = decoder_image_height(decoder);

        image_frame image_frame{.id = f.id,
                                .pixel_format = _descriptor.pixel_format,
                                .width = (uint16_t)decoder_image_width(decoder),
                                .height = (uint16_t)height};
        for (uint8_t i = 0; i < MAX_IMAGE_PLANES; i++) {
          const uint8_t* image_data = decoder_image_data(decoder, i);
          int line_size = decoder_image_line_size(decoder, i);
          image_frame.plane_strides[i] = line_size;

          if (image_data) {
            image_frame.plane_data[i] =
                std::string((const char*)image_data, line_size * height);
          }
        }

        if (!_image_frames_worker->try_send(std::move(image_frame))) {
          tele::counter_inc(image_frames_dropped);
        }
      }
    }
  }

 protected:
  virtual void process_image_frame(image_frame&& frame) {
    const uint8_t* plane_data[MAX_IMAGE_PLANES];
    for (int i = 0; i < MAX_IMAGE_PLANES; ++i) {
      if (frame.plane_data[i].empty()) {
        plane_data[i] = nullptr;
      } else {
        plane_data[i] = (const uint8_t*)frame.plane_data[i].data();
      }
    }
    _descriptor.img_callback(*this, frame.width, frame.height, plane_data,
                             frame.plane_strides);
    send_messages(frame.id.first, frame.id.second);
  }

  virtual void transmit(const bot_message_kind kind, cbor_item_t* message) = 0;

  void send_messages(int64_t i1, int64_t i2) {
    for (auto&& msg : _message_buffer) {
      cbor_item_t* data = msg.data;

      if (i1 >= 0) {
        cbor_item_t* is = cbor_new_definite_array(2);
        cbor_array_set(is, 0,
                       cbor_move(cbor_build_uint64(static_cast<uint64_t>(i1))));
        cbor_array_set(is, 1,
                       cbor_move(cbor_build_uint64(static_cast<uint64_t>(i2))));
        cbor_map_add(data, {.key = cbor_move(cbor_build_string("i")),
                            .value = cbor_move(is)});
      }

      if (!_bot_id.empty()) {
        cbor_map_add(data,
                     {.key = cbor_move(cbor_build_string("from")),
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
  metadata _metadata;

  const std::string _bot_id;
  const bot_descriptor _descriptor;
  std::unique_ptr<threaded_worker<encoded_frame>> _encoded_frames_worker;
  std::unique_ptr<threaded_worker<image_frame>> _image_frames_worker;
};

class bot_offline_instance : public bot_instance {
 public:
  bot_offline_instance(const std::string& bot_id,
                       const bot_descriptor& descriptor, std::ostream& analysis,
                       std::ostream& debug, rtm::video::bot_environment& env)
      : bot_instance(bot_id, descriptor, env),
        _analysis(analysis),
        _debug(debug) {}

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

class bot_online_instance : public bot_instance,
                            public rtm::subscription_callbacks {
 public:
  bot_online_instance(const std::string& bot_id,
                      const bot_descriptor& descriptor,
                      const std::string& channel,
                      rtm::video::bot_environment& env)
      : bot_instance(bot_id, descriptor, env), _channels(channel) {}

  void subscribe_to_control_channel(rtm::subscriber& s) {
    if (!_bot_id.empty() && _descriptor.ctrl_callback) {
      s.subscribe_channel(_channels.control, _control_subscription, *this);
    }
  }

  void on_error(error e, const std::string& msg) override {
    std::cerr << "ERROR: " << (int)e << " " << msg << "\n";
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
  void process_image_frame(image_frame&& frame) override {
    stopwatch<> s;
    bot_instance::process_image_frame(std::move(frame));
    tele::distribution_add(processing_times_millis, s.millis());
  }

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
      std::cerr << "ERROR: Unsupported kind of message\n";
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
                     (struct cbor_pair){
                         .key = cbor_move(cbor_build_string("request_id")),
                         .value = cbor_move(cbor_copy(request_id))});
      }

      queue_message(bot_message_kind::CONTROL, cbor_move(response));
    }

    send_messages(-1, -1);
    return;
  }

  const channel_names _channels;
  const rtm::subscription _control_subscription{};
};

cbor_item_t* configure_command(cbor_item_t* config) {
  cbor_item_t* cmd = cbor_new_definite_map(2);
  cbor_map_add(cmd, (struct cbor_pair){
                        .key = cbor_move(cbor_build_string("action")),
                        .value = cbor_move(cbor_build_string("configure"))});
  cbor_map_add(cmd,
               (struct cbor_pair){.key = cbor_move(cbor_build_string("body")),
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
      "appkey", po::value<std::string>(), "app key")(
      "channel", po::value<std::string>(), "channel")(
      "port", po::value<std::string>(), "port")(
      "time_limit", po::value<double>(), "time in seconds");

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

  bool online_mode =
      (vm.count("endpoint") || vm.count("appkey") || vm.count("channel") ||
       vm.count("port") || vm.count("time_limit"));

  bool offline_mode = (vm.count("video_file") || vm.count("analysis_file") ||
                       vm.count("debug_file") || vm.count("replay_file"));

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
    return;
  }

  cbor_item_t* config;

  if (config_file.is_initialized()) {
    FILE* fp = fopen(config_file.get().c_str(), "r");
    assert(fp);
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
    _bot_instance->queue_message(bot_message_kind::DEBUG, cbor_move(response));
  }
}

int bot_environment::main_online(variables_map cmd_args) {
  const std::string id = cmd_args["id"].as<std::string>();
  const std::string channel = cmd_args["channel"].as<std::string>();
  bot_online_instance* _bot_online_instance =
      new bot_online_instance(id, *_bot_descriptor, channel, *this);
  _bot_instance.reset(_bot_online_instance);

  parse_config(
      cmd_args.count("config")
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
  signals.async_wait(boost::bind(&boost::asio::io_service::stop, &io_service));

  _client = std::make_shared<resilient_client>(
      [&endpoint, &port, &appkey, &io_service, &ssl_context, this]() {
        return rtm::new_client(endpoint, port, appkey, io_service, ssl_context,
                               1, *this);
      });
  _bot_online_instance->subscribe_to_control_channel(*_client);
  _source = std::make_shared<flow_rtm_aggregator>(
      std::make_unique<rtm_source>(_client, channel));
  int err = _source->init();
  if (err) {
    std::cerr << "*** Error initializing video source, error code " << err
              << "\n";
    exit(1);
  }
  _source->subscribe(_bot_instance);
  _source->start();

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
  return 0;
}

int bot_environment::main_offline(variables_map cmd_args) {
  const std::string id = cmd_args["id"].as<std::string>();
  rtm::video::initialize_source_library();
  std::ostream *_analysis, *_debug;
  if (cmd_args.count("analysis_file")) {
    _analysis =
        new std::ofstream(cmd_args["analysis_file"].as<std::string>().c_str());
  } else
    _analysis = &std::cout;
  if (cmd_args.count("debug_file")) {
    _debug =
        new std::ofstream(cmd_args["debug_file"].as<std::string>().c_str());
  } else
    _debug = &std::cerr;

  bot_offline_instance* _bot_offline_instance = new bot_offline_instance(
      id, *_bot_descriptor, *_analysis, *_debug, *this);
  _bot_instance.reset(_bot_offline_instance);

  parse_config(
      cmd_args.count("config")
          ? boost::optional<std::string>{cmd_args["config"].as<std::string>()}
          : boost::optional<std::string>{});

  if (cmd_args.count("video_file"))
    _source = std::make_shared<file_source>(
        cmd_args["video_file"].as<std::string>(), false,
        cmd_args["synchronous"].as<bool>());
  else {
    _source =
        std::make_shared<flow_rtm_aggregator>(std::make_unique<replay_source>(
            cmd_args["replay_file"].as<std::string>(),
            cmd_args["synchronous"].as<bool>()));
  }

  int err = _source->init();
  if (err) {
    std::cerr << "*** Error initializing video source, error code " << err
              << "\n";
    exit(1);
  }
  _source->subscribe(_bot_instance);
  _source->start();
  _analysis->flush();
  _debug->flush();

  return 0;
}  // namespace video

int bot_environment::main(int argc, char* argv[]) {
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
                           cbor_item_t* message) {
  BOOST_ASSERT_MSG(cbor_map_is_indefinite(message),
                   "Message must be indefinite map");
  static_cast<rtm::video::bot_instance&>(ctx).queue_message(kind, message);
}

void rtm_video_bot_register(const bot_descriptor& bot) {
  rtm::video::bot_environment::instance().register_bot(&bot);
}

int rtm_video_bot_main(int argc, char* argv[]) {
  return rtm::video::bot_environment::instance().main(argc, argv);
}
