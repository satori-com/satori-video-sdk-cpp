#include <librtmvideo/video_bot.h>
#include <iostream>

int main() {
  satori::video::bot_register({satori::video::image_pixel_format::BGR, nullptr, nullptr});
  std::cout << "all good, includes and linking is fine, congrats!" << '\n';
  return 0;
}
