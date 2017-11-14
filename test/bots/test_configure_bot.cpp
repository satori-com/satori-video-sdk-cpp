#include <cbor_tools.h>
#include <satorivideo/video_bot.h>
#include <iostream>

#include "logging.h"

namespace sv = satori::video;

namespace test_bot {

struct bot_state {
  int magic_number;
};

cbor_item_t *build_message(const std::string &text) {
  cbor_item_t *message = cbor_new_indefinite_map();
  cbor_map_add(message, {cbor_move(cbor_build_string("message")),
                         cbor_move(cbor_build_string(text.c_str()))});
  return cbor_move(message);
}

void process_image(sv::bot_context &context, const sv::image_frame & /*frame*/) {
  CHECK(context.instance_data != nullptr);  // Make sure initialization passed
  CHECK(context.frame_metadata->width != 0);
  std::cout << "got frame " << context.frame_metadata->width << "x"
            << context.frame_metadata->height << ", BGR stride "
            << context.frame_metadata->plane_strides[0] << "\n";
  sv::bot_message(context, sv::bot_message_kind::ANALYSIS,
                  build_message("test_analysis_message"));
  sv::bot_message(context, sv::bot_message_kind::DEBUG,
                  build_message("test_debug_message"));
}

cbor_item_t *process_command(sv::bot_context &ctx, cbor_item_t *config) {
  auto action = cbor::map(config).get_str("action");

  if (action == "configure") {
    CHECK(ctx.instance_data == nullptr);  // Make sure is has initialized once
    std::cout << "bot is initializing, libraries are ok" << '\n';
    std::string p = cbor::map(config).get_map("body").get_str("myparam", "");
    CHECK_EQ(p, "myvalue");
    ctx.instance_data = new bot_state;
  } else if (action == "shutdown") {
    std::cout << "bot is shutting down\n";
    delete static_cast<bot_state*>(ctx.instance_data);
  }

  return nullptr;
}

}  // namespace test_bot

int main(int argc, char *argv[]) {
  sv::bot_register(sv::bot_descriptor{
      sv::image_pixel_format::BGR, &test_bot::process_image, &test_bot::process_command});
  return sv::bot_main(argc, argv);
}
