#pragma once

#include <rapidjson/document.h>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <gsl/gsl>
#include <memory>

#include "librtmvideo/data.h"
#include "librtmvideo/video_bot.h"
#include "rtmclient.h"
#include "video_streams.h"

namespace rtm {
namespace video {

struct bot_instance;

using variables_map = boost::program_options::variables_map;

class bot_environment : private error_callbacks {
 public:
  static bot_environment& instance();

  void register_bot(const bot_descriptor* bot);
  int main(int argc, char* argv[]);

  void on_metadata(const rapidjson::Value& msg);
  void on_frame_data(const rapidjson::Value& msg);
  rtm::publisher& publisher() { return *_client; }

 private:
  void parse_config(boost::optional<std::string> config_file);
  int main_online(variables_map cmd_args);
  int main_offline(variables_map cmd_args);

  const bot_descriptor* _bot_descriptor{nullptr};
  std::shared_ptr<bot_instance> _bot_instance;
  std::shared_ptr<rtm::client> _client;
  streams::publisher<owned_image_packet> _source;
};

}  // namespace video
}  // namespace rtm
