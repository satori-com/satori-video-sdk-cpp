#pragma once

#include <json.hpp>
#include <list>
#include <memory>

#include "cli_streams.h"
#include "data.h"
#include "metrics.h"
#include "pool_controller.h"
#include "rtm_client.h"
#include "satorivideo/multiframe/bot.h"
#include "video_streams.h"

namespace satori {
namespace video {

struct bot_instance;
struct bot_instance_builder;

struct bot_message {
  nlohmann::json data;
  bot_message_kind kind;
  frame_id id;
};

namespace po = boost::program_options;

struct bot_configuration {
  bot_configuration(const po::variables_map& vm);
  bot_configuration(const nlohmann::json& config);

  const std::string id;
  const boost::optional<std::string> analysis_file;
  const boost::optional<std::string> debug_file;
  const cli_streams::input_video_config video_cfg;
  const nlohmann::json bot_config;
};

class bot_environment : public job_controller,
                        private rtm::error_callbacks,
                        boost::static_visitor<void> {
 public:
  static bot_environment& instance();

  void register_bot(const multiframe_bot_descriptor& bot);
  int main(int argc, char* argv[]);

  rtm::publisher& publisher() { return *_rtm_client; }

  void operator()(const owned_image_metadata& metadata);
  void operator()(const owned_image_frame& frame);
  void operator()(struct bot_message& msg);

  void add_job(const nlohmann::json& job) override;
  void remove_job(const nlohmann::json& job) override;
  nlohmann::json list_jobs() const override;

 private:
  void start_bot(const bot_configuration& config);
  void on_error(std::error_condition ec) override;

  bool _finished;
  uint64_t _multiframes_counter{0};
  nlohmann::json _job;
  metrics_config _metrics_config;
  boost::asio::io_service _io_service;
  multiframe_bot_descriptor _bot_descriptor;
  std::unique_ptr<bot_instance> _bot_instance;
  std::shared_ptr<rtm::client> _rtm_client;
  bool _pool_mode{false};

  streams::observer<nlohmann::json>* _analysis_sink;
  streams::observer<nlohmann::json>* _debug_sink;
  streams::observer<nlohmann::json>* _control_sink;

  std::unique_ptr<std::ofstream> _analysis_file;
  std::unique_ptr<std::ofstream> _debug_file;

  // TODO: maybe make them local variables?
  streams::publisher<std::queue<owned_image_packet>> _source;
  streams::publisher<nlohmann::json> _control_source;
};

}  // namespace video
}  // namespace satori
