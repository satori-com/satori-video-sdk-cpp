#pragma once

#include <cbor.h>
#include <functional>
#include <iostream>
#include <opencv2/opencv.hpp>

namespace satori {
namespace video {
namespace cvbot {

// Structures
struct Vector {
  cv::Point2d start, end;
};

// Drawing functions
void draw(cv::Mat &image, const cv::RotatedRect &rect, const cv::Scalar &color);
void draw(cv::Mat &image, const cv::Rect &rect, const cv::Scalar &color);
template <typename A>
void draw_move(cv::Mat &image, const A &a, const A &b, const cv::Scalar &color);

// Debugging functions
void log_image(const cv::Mat &image);

// Geometry-ordering functions
inline double distance(const cv::Rect &a, const cv::Rect &b);
inline double distance(const cv::Point &a, const cv::Point &b);
inline double distance(const cv::Point2f &a, const cv::Point2f &b);
inline double distance(const cv::Point2d &a, const cv::Point2d &b);
inline double distance(const cv::RotatedRect &a, const cv::RotatedRect &b);
inline cv::Point center(const cv::Rect &a);
inline cv::Point center(const cv::RotatedRect &a);
inline double ordering_value(const cv::Rect &a);
inline double ordering_value(const cv::RotatedRect &a);

bool collinear(const Vector &a, const Vector &b, double precision);

template <typename A>
int closest(const std::vector<A> &h1, int e2, const std::vector<A> &h2,
            double MAX_DISTANCE);

template <typename A>
std::function<bool(A, A)> ordering_function();

struct LogRecord {
  std::vector<cv::Point2d> points;
  uint32_t groupId;
  std::string caption;
  uint32_t thickness;
};

class debug_logger {
public:
  debug_logger(bot_context *ctx);
  void set_image(cv::Mat *img);
  void add(const std::vector<cv::Point2d> &points, uint32_t groupId,
           const std::string &caption, uint32_t thickness = 1);
  ~debug_logger();

private:
  cv::Mat *image{nullptr};
  bot_context *context{nullptr};
  std::vector<LogRecord> records;
};

}  // namespace cvbot
}  // namespace video
}  // namespace satori

#include "opencv_impl.h"
