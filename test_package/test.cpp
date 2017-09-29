#include <librtmvideo/video_bot.h>
#include <iostream>

int main() {
  rtm_video_bot_register(
      bot_descriptor{640, 480, image_pixel_format::BGR, nullptr, nullptr});
  std::cout << "all good, includes and linking is fine, congrats!" << '\n';
  return 0;
}
