#include <cbor_tools.h>
#include <librtmvideo/video_bot.h>
#include <iostream>

#include "logging.h"

namespace test_bot {
struct State {
  int magic_number;
};
cbor_item_t *build_message(const std::string &text) {
  cbor_item_t *message = cbor_new_indefinite_map();
  cbor_map_add(message, {cbor_move(cbor_build_string("message")),
                         cbor_move(cbor_build_string(text.c_str()))});
  return cbor_move(message);
}
void process_image(bot_context &context, const image_frame &frame) {
  CHECK(context.instance_data != nullptr);  // Make sure initialization passed
  CHECK(context.frame_metadata->width != 0);
  std::cout << "got frame " << context.frame_metadata->width << "x"
            << context.frame_metadata->height << ", BGR stride "
            << context.frame_metadata->plane_strides[0] << "\n";
  rtm_video_bot_message(context, bot_message_kind::ANALYSIS,
                        cbor_move(build_message("test_analysis_message")));
  rtm_video_bot_message(context, bot_message_kind::DEBUG,
                        cbor_move(build_message("test_debug_message")));
}
cbor_item_t *process_command(bot_context &ctx, cbor_item_t *config) {
  if (cbor::map_has_str_value(config, "action", "configure")) {
    CHECK(ctx.instance_data == nullptr);  // Make sure is has initialized once
    State *state = new State;
    std::cout << "bot is initializing, libraries are ok" << '\n';
    std::string p = cbor::map(config).get_map("body").get_str("myparam", "");
    CHECK_EQ(p, "myvalue");
    ctx.instance_data = state;
  }
  return nullptr;
}
}  // namespace test_bot

int main(int argc, char *argv[]) {
  rtm_video_bot_register(bot_descriptor{640, 480, image_pixel_format::BGR,
                                        &test_bot::process_image,
                                        &test_bot::process_command});
  return rtm_video_bot_main(argc, argv);
}
