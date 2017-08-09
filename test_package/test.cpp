#include <assert.h>
#include <librtmvideo/video_bot.h>
#include <iostream>

namespace verifier {
void process_image(bot_context &context, const uint8_t *image, uint16_t width,
                   uint16_t height, uint16_t linesize) {
  std::cout << "got frame " << width << "x" << height << "\n";
}
cbor_item_t *process_command(bot_context &, cbor_item_t *) {
  std::cout << "bot is initializing, libraries are ok" << '\n';
  return nullptr;
}
}  // namespace verifier

int main(int argc, char *argv[]) {
  rtm_video_bot_register(bot_descriptor{640, 480, image_pixel_format::BGR,
                                        &verifier::process_image,
                                        &verifier::process_command});
  std::cout << "all good, includes and linking is fine, congrads!" << '\n';
  return 0;
}
