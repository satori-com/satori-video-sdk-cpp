#pragma once

#include <list>

#include "bot_environment.h"
#include "data.h"
#include "satorivideo/video_bot.h"
#include "streams/streams.h"

namespace satori {
namespace video {

class bot_instance : public bot_context, streams::subscriber<owned_image_packet>,
                     boost::static_visitor<void> {
 public:
  bot_instance(const std::string& bot_id, execution_mode execmode,
               const bot_descriptor& descriptor, bot_environment& env);
  ~bot_instance() override;

  void configure(cbor_item_t* config);

  void start(streams::publisher<owned_image_packet>& video_stream,
             streams::publisher<cbor_item_t*>& control_stream);
  void stop();

  void queue_message(bot_message_kind kind, cbor_item_t* message, const frame_id& id);

  void operator()(const owned_image_metadata& metadata);
  void operator()(const owned_image_frame& frame);
  void operator()(cbor_item_t* msg);

private:
  struct control_sub;

  void on_next(owned_image_packet&& packet) override;
  void on_error(std::error_condition ec) override;
  void on_complete() override;
  void on_subscribe(streams::subscription& s) override;

  void send_messages(const frame_id& id);

  const std::string _bot_id;
  const bot_descriptor _descriptor;

  bot_environment& _env;
  std::list<struct bot_message> _message_buffer;
  image_metadata _image_metadata{0, 0};

  streams::subscription* _video_sub{nullptr};
  std::unique_ptr<control_sub> _control_sub;
};

}  // namespace video
}  // namespace satori
