#include <satorivideo/video_bot.h>
#include <iostream>

#include <json.hpp>

#define LOGURU_WITH_STREAMS 1
#include <loguru/loguru.hpp>

namespace sv = satori::video;

namespace empty_bot {

void process_image(sv::bot_context &context, const sv::image_frame & /*frame*/) {
  LOG_S(INFO) << "got frame " << context.frame_metadata->width << "x"
              << context.frame_metadata->height;
  sv::bot_message(context, sv::bot_message_kind::ANALYSIS, {{"msg", "hello"}});
}

}  // namespace empty_bot

int main(int argc, char *argv[]) {
  sv::bot_register(
      sv::bot_descriptor{sv::image_pixel_format::BGR, &empty_bot::process_image});
  return sv::bot_main(argc, argv);
}
