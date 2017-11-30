#pragma once

#include <list>

#include "bot_environment.h"
#include "data.h"
#include "satorivideo/video_bot.h"
#include "streams/streams.h"
#include "variant_utils.h"

namespace satori {
namespace video {

using bot_input = variantutils::extend_variant<owned_image_packet, cbor_item_t*>::type;
using bot_output =
    variantutils::extend_variant<owned_image_packet, struct bot_message>::type;

class bot_instance : public bot_context, boost::static_visitor<std::list<bot_output>> {
 public:
  bot_instance(const std::string& bot_id, execution_mode execmode,
               const bot_descriptor& descriptor);
  ~bot_instance() = default;

  void configure(cbor_item_t* config);

  streams::op<bot_input, bot_output> run_bot();

  void queue_message(bot_message_kind kind, cbor_item_t* message, const frame_id& id);

  std::list<bot_output> operator()(const owned_image_metadata& metadata);
  std::list<bot_output> operator()(const owned_image_frame& frame);
  std::list<bot_output> operator()(cbor_item_t* msg);

 private:
  void prepare_message_buffer_for_downstream(const frame_id& id);

  const std::string _bot_id;
  const bot_descriptor _descriptor;

  std::list<struct bot_message> _message_buffer;
  image_metadata _image_metadata{0, 0};
};

}  // namespace video
}  // namespace satori
