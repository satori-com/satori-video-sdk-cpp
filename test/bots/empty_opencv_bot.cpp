#include <satorivideo/opencv/opencv_bot.h>

#define LOGURU_WITH_STREAMS 1
#include <loguru/loguru.hpp>

namespace sv = satori::video;

namespace empty_bot {

void process_image(sv::bot_context & /*context*/, const cv::Mat &mat) {
  LOG_S(1) << "got frame " << mat.size;
}

}  // namespace empty_bot

int main(int argc, char *argv[]) {
  sv::opencv_bot_register({&empty_bot::process_image});
  return sv::opencv_bot_main(argc, argv);
}
