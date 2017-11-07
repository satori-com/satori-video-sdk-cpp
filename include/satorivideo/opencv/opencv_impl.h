#pragma once

#include <cbor.h>
#include <iostream>
#include <opencv2/opencv.hpp>

namespace satori {
namespace video {
namespace opencv {

template <typename A>
std::function<bool(A, A)> ordering_function() {
  return [](const A &a, const A &b) { return ordering_value(a) < ordering_value(b); };
}

inline cv::Point center(const cv::Rect &a) {
  return cv::Point(a.x + a.width * 0.5, a.y + a.height * 0.5);
}

inline cv::Point center(const cv::RotatedRect &a) { return a.center; }

inline double ordering_value(const cv::Rect &a) { return a.y; }
inline double ordering_value(const cv::RotatedRect &a) { return a.center.y; }

inline double distance(const cv::RotatedRect &a, const cv::RotatedRect &b) {
  return distance(a.center, b.center);
}

inline double distance(const cv::Point &a, const cv::Point &b) {
  return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

inline double distance(const cv::Point2f &a, const cv::Point2f &b) {
  return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

inline double distance(const cv::Point2d &a, const cv::Point2d &b) {
  return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

inline double distance(const cv::Rect &a, const cv::Rect &b) {
  return sqrt((a.x + a.width * 0.5 - b.x - b.width * 0.5)
                  * (a.x + a.width * 0.5 - b.x - b.width * 0.5)
              + (a.y + a.height * 0.5 - b.y - b.height * 0.5)
                    * (a.y + a.height * 0.5 - b.y - b.height * 0.5));
}

template <typename A>
void draw_move(cv::Mat &image, const A &a, const A &b, const cv::Scalar &color) {
  draw(image, a, color);
  draw(image, b, color);
  // better with arrowedLine, downgraded to line because of too old Ubuntu
  // OpenCV
  cv::line(image, center(a), center(b), color, 3);
}

// Find and return element index e1 in in collection h1 which is closest to e2
// collection should be ordered!
template <typename A>
int closest(const std::vector<A> &h1, int e2, const std::vector<A> &h2,
            double MAX_DISTANCE) {
  if (e2 == -1) {
    return -1;
  }
  A a = h2[e2];
  int e1 = -1;
  double min = 0.0;
  for (int i = 0; i < h1.size(); i++) {
    if (e1 == -1 || distance(h1[i], a) < min) {
      e1 = i;
      min = distance(h1[i], a);
    }
    if (ordering_value(h1[i]) > ordering_value(a) + MAX_DISTANCE) {
      break;
    }
  }
  if (min > MAX_DISTANCE) {
    return -1;
  }
  return e1;
}
}  // namespace opencv
}  // namespace video
}  // namespace satori
