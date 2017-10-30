#include <satorivideo/video_bot.h>
#include <iostream>
#include <thread>

using namespace satori::video;

namespace test_bot {

void process_image(bot_context &context, const image_frame &frame) {
  std::cout << "got frame " << context.frame_metadata->width << "x"
            << context.frame_metadata->height << "\n";
}

}  // namespace test_bot

int main(int argc, char *argv[]) {
  bot_register(bot_descriptor{image_pixel_format::BGR, &test_bot::process_image});
  return bot_main(argc, argv);
}
