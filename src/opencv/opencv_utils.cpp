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

bool collinear(const Vector &a, const Vector &b, double precision) {
  if (distance(a.start, a.end) == 0.0 || distance(b.start, b.end) == 0.0) {
    return false;
  }
  cv::Point2f an(((double)a.end.x - a.start.x) / distance(a.start, a.end),
                 ((double)a.end.y - a.start.y) / distance(a.start, a.end));
  cv::Point2f bn(((double)b.end.x - b.start.x) / distance(b.start, b.end),
                 ((double)b.end.y - b.start.y) / distance(b.start, b.end));
  return distance(an, bn) < precision;
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

void draw(cv::Mat &image, const cv::RotatedRect &rect, cv::Scalar const &color) {
  cv::Point2f rect_points[4];
  rect.points(rect_points);
  for (size_t j = 0; j < 4; j++) {
    cv::line(image, rect_points[j], rect_points[(j + 1) % 4], color);
  }
}

void draw(cv::Mat &image, const cv::Rect &rect, const cv::Scalar &color) {
  cv::rectangle(image, rect, color);
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

debug_logger::debug_logger(bot_context *ctx) : context{ctx} {}
void debug_logger::set_image(cv::Mat *img) { image = img; }
void debug_logger::add(const std::vector<cv::Point2d> &points, uint32_t groupId,
                       const std::string &caption, uint32_t thickness) {
  records.push_back(LogRecord{points, groupId, caption, thickness});
}

cbor_pair kv_pair(const std::string &key, cbor_item_t *value) {
  return {cbor_move(cbor_build_string(key.c_str())), cbor_move(value)};
}

cbor_item_t *points(const std::vector<cv::Point2d> &plist) {
  cbor_item_t *cbarray = cbor_new_definite_array(plist.size() * 2);
  for (cv::Point p : plist) {
    cbor_array_push(cbarray, cbor_move(cbor_build_float8(p.x)));
    cbor_array_push(cbarray, cbor_move(cbor_build_float8(p.y)));
  }
  return cbarray;
}

cv::Scalar id_color(uint32_t id) {
  return {static_cast<double>((id * 200) % 256), static_cast<double>((id * 150) % 256),
          static_cast<double>((255 + id * 100) % 256)};
}

debug_logger::~debug_logger() {
  if (context != nullptr) {
    cbor_item_t *message = cbor_new_definite_array(records.size());
    for (LogRecord &entry : records) {
      cbor_item_t *cbmap = cbor_new_definite_map(4);
      cbor_map_add(cbmap, kv_pair("caption", cbor_build_string(entry.caption.c_str())));
      cbor_map_add(cbmap, kv_pair("groupId", cbor_build_uint32(entry.groupId)));
      cbor_map_add(cbmap, kv_pair("thickness", cbor_build_uint32(entry.thickness)));
      cbor_map_add(cbmap, kv_pair("points", points(entry.points)));
      cbor_array_push(message, cbor_move(cbmap));
    }
    bot_message(*context, bot_message_kind::DEBUG, cbor_move(message));
  }

  if (image != nullptr) {
    for (LogRecord &entry : records) {
      if (entry.points.size() > 1) {
        for (size_t i = 0; i < entry.points.size() - 1; i++) {
          cv::line(*image, entry.points[i], entry.points[i + 1], id_color(entry.groupId),
                   entry.thickness);
        }
      }
      if (!entry.caption.empty()) {
        cv::putText(*image, entry.caption, entry.points[0], cv::FONT_HERSHEY_PLAIN, 1,
                    id_color(entry.groupId), entry.thickness);
      }
    }
    log_image(*image);
  }
}
}  // namespace opencv
}  // namespace video
}  // namespace satori
