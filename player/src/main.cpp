#include <librtmvideo/video_bot.h>
#include <opencv2/opencv.hpp>
#include <thread>

namespace player {
void process_image(bot_context &context, const uint8_t *image, uint16_t width,
                   uint16_t height, uint16_t linesize) {
  // Read image
  cv::Mat original(height, width, CV_8UC3, (void *)image, linesize);
  cv::imshow("Player", original);
}
}  // namespace player

int main(int argc, char *argv[]) {
  rtm_video_bot_register(bot_descriptor{640, 480, image_pixel_format::BGR,
                                        &player::process_image, nullptr});
  cv::namedWindow("Player");
  std::thread([argc, argv] { rtm_video_bot_main(argc, argv); }).detach();
  while (true) {
    // it is required to make image appear
    // delay 10 ms because it couldn't be more than 100 rps
    cv::waitKey(10);
  }
}
