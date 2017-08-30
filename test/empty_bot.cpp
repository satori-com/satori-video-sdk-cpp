#include <assert.h>
#include <librtmvideo/cbor_tools.h>
#include <librtmvideo/video_bot.h>
#include <iostream>

namespace empty_bot {

void process_image(bot_context &context, uint16_t width, uint16_t height,
                   const uint8_t *plane_data[MAX_IMAGE_PLANES],
                   const uint32_t plane_strides[MAX_IMAGE_PLANES]) {
  std::cout << "got frame " << width << "x" << height << "\n";
}
cbor_item_t *process_command(bot_context &ctx, cbor_item_t *config) {
  return nullptr;
}

}  // namespace empty_bot

int main(int argc, char *argv[]) {
  rtm_video_bot_register(bot_descriptor{640, 480, image_pixel_format::BGR,
                                        &empty_bot::process_image,
                                        &empty_bot::process_command});
  return rtm_video_bot_main(argc, argv);
}
