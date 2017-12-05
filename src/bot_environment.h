#pragma once

#include <list>
#include <memory>

#include "data.h"
#include "rtm_client.h"
#include "satorivideo/multiframe/bot.h"
#include "video_streams.h"

namespace satori {
namespace video {

struct bot_instance;

struct bot_message {
  cbor_item_t* data;
  bot_message_kind kind;
  frame_id id;
};

class bot_environment : private rtm::error_callbacks, boost::static_visitor<void> {
 public:
  static bot_environment& instance();

  void register_bot(const multiframe_bot_descriptor& bot);
  int main(int argc, char* argv[]);

  rtm::publisher& publisher() { return *_rtm_client; }

  void operator()(const owned_image_metadata& metadata);
  void operator()(const owned_image_frame& frame);
  void operator()(struct bot_message& msg);

 private:
  void on_error(std::error_condition ec) override;
  multiframe_bot_descriptor _bot_descriptor;
  std::shared_ptr<bot_instance> _bot_instance;
  std::shared_ptr<rtm::client> _rtm_client;

  streams::observer<cbor_item_t*>* _analysis_sink;
  streams::observer<cbor_item_t*>* _debug_sink;
  streams::observer<cbor_item_t*>* _control_sink;

  std::unique_ptr<std::ofstream> _analysis_file;
  std::unique_ptr<std::ofstream> _debug_file;

  // TODO: maybe make them local variables?
  streams::publisher<std::queue<owned_image_packet>> _source;
  streams::publisher<cbor_item_t*> _control_source;
};

}  // namespace video
}  // namespace satori
