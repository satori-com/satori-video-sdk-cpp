#include <satorivideo/opencv/opencv_bot.h>

namespace sv = satori::video;

namespace test_bot {

void process_image(sv::bot_context &context, const cv::Mat &mat) {
  std::cout << "got frame " << mat.size << "\n";
}

}  // namespace test_bot

int main(int argc, char *argv[]) {
  sv::opencv_bot_register({&test_bot::process_image});
  return sv::opencv_bot_main(argc, argv);
}
