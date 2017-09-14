#pragma once

#include <rapidjson/document.h>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <gsl/gsl>
#include <memory>

#include "librtmvideo/data.h"
#include "librtmvideo/video_bot.h"
#include "rtmclient.h"
#include "tele_impl.h"
#include "video_streams.h"

namespace rtm {
namespace video {

struct bot_instance;

using variables_map = boost::program_options::variables_map;

struct bot_message {
  cbor_item_t* data;
  bot_message_kind kind;
  frame_id id;
};

class bot_environment : private error_callbacks {
 public:
  static bot_environment& instance();

  void register_bot(const bot_descriptor* bot);
  int main(int argc, char* argv[]);

  void on_metadata(const rapidjson::Value& msg);
  void on_frame_data(const rapidjson::Value& msg);
  rtm::publisher& publisher() { return *_rtm_client; }

  void send_messages(std::list<bot_message>&& messages);

 private:
  void parse_config(boost::optional<std::string> config_file);
  void stop(boost::asio::io_service& io);

  const bot_descriptor* _bot_descriptor{nullptr};
  std::shared_ptr<bot_instance> _bot_instance;
  std::shared_ptr<rtm::client> _rtm_client;
  std::unique_ptr<tele::publisher> _tele_publisher;

  streams::observer<cbor_item_t*>* _analysis_sink;
  streams::observer<cbor_item_t*>* _debug_sink;
  streams::observer<cbor_item_t*>* _control_sink;

  std::unique_ptr<std::ofstream> _analysis_file;
  std::unique_ptr<std::ofstream> _debug_file;

  streams::publisher<owned_image_packet> _source;
  streams::publisher<cbor_item_t*> _control_source;
};

}  // namespace video
}  // namespace rtm
