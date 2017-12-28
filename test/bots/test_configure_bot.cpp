#include <satorivideo/video_bot.h>
#include <iostream>

#include "logging.h"

namespace sv = satori::video;

namespace test_bot {

struct bot_state {
  int magic_number;
};

void process_image(sv::bot_context &context, const sv::image_frame & /*frame*/) {
  CHECK(context.instance_data != nullptr);  // Make sure initialization passed
  CHECK(context.frame_metadata->width != 0);
  std::cout << "got frame " << context.frame_metadata->width << "x"
            << context.frame_metadata->height << ", BGR stride "
            << context.frame_metadata->plane_strides[0] << "\n";
  sv::bot_message(context, sv::bot_message_kind::ANALYSIS,
                  {{"message", "test_analysis_message"}});
  sv::bot_message(context, sv::bot_message_kind::DEBUG,
                  {{"message", "test_debug_message"}});
}

nlohmann::json process_command(sv::bot_context &ctx, const nlohmann::json &config) {
  auto &action = config["action"];

  if (action == "configure") {
    CHECK(ctx.instance_data == nullptr);  // Make sure is has initialized once
    std::cout << "bot is initializing, libraries are ok" << '\n';
    std::string p = config["body"]["myparam"];
    CHECK_EQ(p, "myvalue");
    ctx.instance_data = new bot_state;
  } else if (action == "shutdown") {
    std::cout << "bot is shutting down\n";
    delete static_cast<bot_state *>(ctx.instance_data);
    return {{"message", "test_shutdown_message"}};
  }

  return nullptr;
}

}  // namespace test_bot

int main(int argc, char *argv[]) {
  sv::bot_register(sv::bot_descriptor{
      sv::image_pixel_format::BGR, &test_bot::process_image, &test_bot::process_command});
  return sv::bot_main(argc, argv);
}
