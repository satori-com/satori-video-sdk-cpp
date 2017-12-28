#pragma once

#include <functional>
#include <iostream>
#include <json.hpp>
#include <opencv2/opencv.hpp>

#include "satorivideo/video_bot.h"

namespace satori {
namespace video {
namespace opencv {

// It is recommended to publish coordinates as fractions of the full frame size.
// For example, if the frame is 100x100,
// fractional coordinates of the point (20, 30) will be (0.2, 0.3)
// following conversion functions are explained with examples

// Example: it converts from (64, 48) -> (0.1, 0.1) for size 640x480
cv::Point2d to_fractional(const cv::Point2d &p, const cv::Size &view);
// Example: it converts from (64, 48, 64, 48) -> (0.1, 0.1, 0.1, 0.1) for size 640x480
cv::Rect2d to_fractional(const cv::Rect2d &rect, const cv::Size &view);
// Example: it converts from  (0.1, 0.1) -> (64, 48) for size 640x480
cv::Point2d from_fractional(const cv::Point2d &p, const cv::Size &view);
// Example: it converts from  (0.1, 0.1, 0.1, 0.1) -> (64, 48, 64, 48) for size 640x480
cv::Rect2d from_fractional(const cv::Rect2d &p, const cv::Size &view);

// JSON output functions
nlohmann::json to_json(cv::Point2d p);
nlohmann::json to_json(cv::Rect2d rect);

// JSON input functions
cv::Point2d point_from_json(const nlohmann::json &item);
cv::Rect2d rect_from_json(const nlohmann::json &item);

// Saves given image as file "logs/frame<number>.jpg", number starts from 1 and
// increases on every function call.
// It will also produce error message if folder "logs" does not exist.
// This function could be helpful for local debugging
// For example, three calls will produce three files:
// logs/frame1.jpg, logs/frame2.jpg and logs/frame3.jpg
void log_image(const cv::Mat &image);

}  // namespace opencv
}  // namespace video
}  // namespace satori
