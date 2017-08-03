#pragma once

#include <gsl/gsl>
#include "librtmvideo/decoder.h"
#include "librtmvideo/rtmvideo.h"
#include "librtmvideo/video_bot.h"
#include "rtmclient.h"

namespace rtm {
namespace video {

struct bot_instance;

class bot_environment : public subscription_callbacks {
 public:
  static bot_environment& instance();

  void register_bot(const bot_descriptor* bot);
  int main(int argc, char* argv[]);

  void on_metadata(const rapidjson::Value& msg);
  void on_frame_data(const rapidjson::Value& msg);
  rtm::publisher& publisher() { return *_client; }

 private:
  void parse_config(const char* config_file);

  const bot_descriptor* _bot_descriptor{nullptr};
  std::unique_ptr<bot_instance> _bot_instance;
  std::unique_ptr<rtm::client> _client;
};

}  // namespace video
}  // namespace rtm
