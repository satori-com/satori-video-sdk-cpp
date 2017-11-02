#include "bot_environment.h"
#include "bot_instance.h"

namespace satori {
namespace video {

void bot_message(bot_context& context, const bot_message_kind kind, cbor_item_t* message,
                 const frame_id& id) {
  CHECK(cbor_map_is_indefinite(message)) << "Message must be indefinite map";
  static_cast<bot_instance&>(context).queue_message(kind, message, id);
}

void bot_register(const bot_descriptor& bot) {
  bot_environment::instance().register_bot(&bot);
}

int bot_main(int argc, char* argv[]) {
  return bot_environment::instance().main(argc, argv);
}

}  // namespace video
}
