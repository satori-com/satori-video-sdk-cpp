#pragma once

#include <boost/program_options.hpp>
#include <gsl/gsl>
#include <list>
#include <memory>

#include "data.h"
#include "rtm_client.h"
#include "satorivideo/video_bot.h"
#include "video_streams.h"

namespace satori {
namespace video {

struct bot_instance;

struct bot_message {
  cbor_item_t* data;
  bot_message_kind kind;
  frame_id id;
};

class bot_environment : private rtm::error_callbacks {
 public:
  static bot_environment& instance();

  void register_bot(const bot_descriptor* bot);
  int main(int argc, char* argv[]);

  rtm::publisher& publisher() { return *_rtm_client; }

  void send_messages(std::list<struct bot_message>&& messages);

 private:
  void process_config(cbor_item_t* bot_config);
  void on_error(std::error_condition ec) override;
  const bot_descriptor* _bot_descriptor{nullptr};
  std::shared_ptr<bot_instance> _bot_instance;
  std::shared_ptr<rtm::client> _rtm_client;

  streams::observer<cbor_item_t*>* _analysis_sink;
  streams::observer<cbor_item_t*>* _debug_sink;
  streams::observer<cbor_item_t*>* _control_sink;

  std::unique_ptr<std::ofstream> _analysis_file;
  std::unique_ptr<std::ofstream> _debug_file;

  streams::publisher<owned_image_packet> _source;
  streams::publisher<cbor_item_t*> _control_source;
};

}  // namespace video
}  // namespace satori
