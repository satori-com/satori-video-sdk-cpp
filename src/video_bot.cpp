#include "bot_environment.h"
#include "bot_instance.h"

void rtm_video_bot_message(bot_context& ctx, const bot_message_kind kind,
                           cbor_item_t* message, const frame_id& id) {
  CHECK(cbor_map_is_indefinite(message)) << "Message must be indefinite map";
  static_cast<rtm::video::bot_instance&>(ctx).queue_message(kind, message, id);
}

void rtm_video_bot_register(const bot_descriptor& bot) {
  rtm::video::bot_environment::instance().register_bot(&bot);
}

int rtm_video_bot_main(int argc, char* argv[]) {
  return rtm::video::bot_environment::instance().main(argc, argv);
}
