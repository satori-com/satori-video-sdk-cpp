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

cbor_item_t *rect_to_cbor(cv::Rect rect) {
  cbor_item_t *coordinates = cbor_new_definite_array(4);
  cbor_array_push(coordinates, cbor_move(cbor_build_float8(rect.x)));
  cbor_array_push(coordinates, cbor_move(cbor_build_float8(rect.y)));
  cbor_array_push(coordinates, cbor_move(cbor_build_float8(rect.width)));
  cbor_array_push(coordinates, cbor_move(cbor_build_float8(rect.height)));
  return coordinates;
}

cbor_item_t *rect_to_cbor(cv::Rect2d rect) {
  cbor_item_t *coordinates = cbor_new_definite_array(4);
  cbor_array_push(coordinates, cbor_move(cbor_build_float8(rect.x)));
  cbor_array_push(coordinates, cbor_move(cbor_build_float8(rect.y)));
  cbor_array_push(coordinates, cbor_move(cbor_build_float8(rect.width)));
  cbor_array_push(coordinates, cbor_move(cbor_build_float8(rect.height)));
  return coordinates;
}

cv::Rect2d rect_from_cbor(cbor_item_t *item) {
  double x1 = cbor_float_get_float(cbor_array_handle(item)[0]);
  double y1 = cbor_float_get_float(cbor_array_handle(item)[1]);
  double x2 = cbor_float_get_float(cbor_array_handle(item)[2]);
  double y2 = cbor_float_get_float(cbor_array_handle(item)[3]);
  return cv::Rect2d(x1, y1, x2 - x1, y2 - y1);
}

cv::Point2d point_from_cbor(cbor_item_t *item) {
  double x = cbor_float_get_float(cbor_array_handle(item)[0]);
  double y = cbor_float_get_float(cbor_array_handle(item)[1]);
  return cv::Point2d(x, y);
}

cv::Rect2d convert_to_fractional(const cv::Rect &rect, const cv::Size &view) {
  return cv::Rect2d(static_cast<double>(rect.x) / view.width,
                    static_cast<double>(rect.y) / view.height,
                    static_cast<double>(rect.width) / view.width,
                    static_cast<double>(rect.height) / view.height);
}

cv::Point2d convert_from_fractional(const cv::Point2d &p, const cv::Size &view) {
  return cv::Point2d(p.x * view.width, p.y * view.height);
}

cv::Rect2d convert_from_fractional(const cv::Rect2d &p, const cv::Size &view) {
  return cv::Rect2d(p.x * view.width, p.y * view.height, p.width * view.width,
                    p.height * view.height);
}
}  // namespace opencv
}  // namespace video
}  // namespace satori
