#pragma once

#include <list>
#include <queue>

#include "bot_environment.h"
#include "data.h"
#include "satorivideo/video_bot.h"
#include "streams/streams.h"
#include "variant_utils.h"

namespace satori {
namespace video {

// Packets are stored in std::queue, the first one is the oldest one
using owned_image_packets = std::queue<owned_image_packet>;
using bot_input = boost::variant<owned_image_packets, cbor_item_t*>;
using bot_output =
    variantutils::extend_variant<owned_image_packet, struct bot_message>::type;

class bot_instance : public bot_context, boost::static_visitor<std::list<bot_output>> {
 public:
  bot_instance(const std::string& bot_id, execution_mode execmode,
               const multiframe_bot_descriptor& descriptor);
  ~bot_instance() = default;

  void configure(cbor_item_t* config);

  streams::op<bot_input, bot_output> run_bot();

  void queue_message(bot_message_kind kind, cbor_item_t* message, const frame_id& id);
  void set_current_frame_id(const frame_id& id);

  std::list<bot_output> operator()(std::queue<owned_image_packet>& pp);
  std::list<bot_output> operator()(cbor_item_t* msg);

 private:
  void prepare_message_buffer_for_downstream();
  std::vector<image_frame> extract_frames(const std::list<bot_output>& packets);

  const std::string _bot_id;
  const multiframe_bot_descriptor _descriptor;

  std::list<struct bot_message> _message_buffer;
  image_metadata _image_metadata{0, 0};
  frame_id _current_frame_id;
};

}  // namespace video
}  // namespace satori
