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
void draw(cv::Mat image, cv::RotatedRect rect, cv::Scalar color);
void draw(cv::Mat image, cv::Rect rect, cv::Scalar color);
template <typename A>
void draw_move(cv::Mat image, A a, A b, cv::Scalar color);

// Debugging functions
void log_image(cv::Mat image);


// Geometry-ordering functions
inline double distance(cv::Rect a, cv::Rect b);
inline double distance(cv::Point a, cv::Point b);
inline double distance(cv::Point2f a, cv::Point2f b);
inline double distance(cv::Point2d a, cv::Point2d b);
inline double distance(cv::RotatedRect a, cv::RotatedRect b);
inline cv::Point center(cv::Rect a);
inline cv::Point center(cv::RotatedRect a);
inline double ordering_value(cv::Rect a);
inline double ordering_value(cv::RotatedRect a);

bool collinear(Vector a, Vector b, double precision);

template <typename A>
int closest(std::vector<A> h1, int e2, std::vector<A> h2, double MAX_DISTANCE);

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
  void add(std::vector<cv::Point2d> points, uint32_t groupId, std::string caption,
           uint32_t thickness = 1);
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
