#include "satorivideo/opencv/opencv_utils.h"
#include "../logging.h"

namespace satori {
namespace video {
namespace opencv {

void log_image(const cv::Mat &image) {
  static int counter = 1;
  std::string filename = "logs/frame" + std::to_string(counter++) + ".jpg";
  if (cv::imwrite(filename, image)) {
    LOG(4) << "Logged: " << filename;
  } else {
    LOG(ERROR) << "Failed to write: " << filename;
  }
}

nlohmann::json to_json(cv::Point2d p) { return nlohmann::json::array({p.x, p.y}); }

nlohmann::json to_json(cv::Rect2d rect) {
  return nlohmann::json::array({rect.x, rect.y, rect.width, rect.height});
}

cv::Rect2d rect_from_json(const nlohmann::json &item) {
  CHECK(item.is_array()) << "item is not an array: " << item;
  CHECK_EQ(4, item.size()) << "should have four elements: " << item;

  const double x1 = item[0];
  const double y1 = item[1];
  const double x2 = item[2];
  const double y2 = item[3];

  return cv::Rect2d(x1, y1, x2 - x1, y2 - y1);
}

cv::Point2d point_from_json(const nlohmann::json &item) {
  CHECK(item.is_array()) << "item is not an array: " << item;
  CHECK_EQ(2, item.size()) << "should have two elements: " << item;

  const double x = item[0];
  const double y = item[1];
  return cv::Point2d(x, y);
}

cv::Point2d to_fractional(const cv::Point2d &p, const cv::Size &view) {
  return cv::Point2d(p.x / view.width, p.y / view.height);
}

cv::Rect2d to_fractional(const cv::Rect2d &rect, const cv::Size &view) {
  return cv::Rect2d(rect.x / view.width, rect.y / view.height, rect.width / view.width,
                    rect.height / view.height);
}

cv::Point2d from_fractional(const cv::Point2d &p, const cv::Size &view) {
  return cv::Point2d(p.x * view.width, p.y * view.height);
}

cv::Rect2d from_fractional(const cv::Rect2d &p, const cv::Size &view) {
  return cv::Rect2d(p.x * view.width, p.y * view.height, p.width * view.width,
                    p.height * view.height);
}
}  // namespace opencv
}  // namespace video
}  // namespace satori
