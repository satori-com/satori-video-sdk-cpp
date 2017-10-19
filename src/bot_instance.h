#pragma once

#include <boost/mpl/vector.hpp>
#include <list>

#include "data.h"
#include "satorivideo/video_bot.h"
#include "streams/streams.h"

namespace satori {
namespace video {

struct bot_message {
  cbor_item_t* data;
  bot_message_kind kind;
  frame_id id;
};

using bot_instance_input_types =
    boost::mpl::push_front<owned_image_packet::types, cbor_item_t*>::type;

using bot_instance_input = boost::make_variant_over<bot_instance_input_types>::type;

using bot_instance_output_types =
    boost::mpl::push_front<owned_image_packet::types, struct bot_message>::type;

using bot_instance_output = boost::make_variant_over<bot_instance_output_types>::type;

class bot_instance : public bot_context {
 public:
  bot_instance(const std::string& bot_id, const execution_mode execmode,
               const bot_descriptor& descriptor);
  ~bot_instance();

  streams::op<bot_instance_input, bot_instance_output> process();

  // TODO: maybe remove usage of this function from bot_environment.cpp
  void queue_message(const bot_message_kind kind, cbor_item_t* message,
                     const frame_id& id);

 private:
  // TODO: turn into visitors
  void process_image_metadata(const owned_image_metadata& metadata);
  void process_image_frame(const owned_image_frame& frame);
  void process_control_message(cbor_item_t* msg);

  void prepare_messages_for_sending(const frame_id& id);

  const std::string _bot_id;
  const bot_descriptor _descriptor;

  std::list<struct bot_message> _message_buffer;
  image_metadata _image_metadata{0, 0};
};

}  // namespace video
}  // namespace satori
