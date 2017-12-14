#pragma once

#include <cbor.h>
#include <satorivideo/video_bot.h>
#include <functional>
#include <iostream>
#include <opencv2/opencv.hpp>

namespace satori {
namespace video {
namespace opencv {

// It is recommended to publish coordinates as fractions of the full frame size.
// For example, if the frame is 100x100,
// fractional coordinates of the point (20, 30) will be (0.2, 0.3)
// following conversion functions are explained with examples

// Example: it converts from (64, 48, 64, 48) -> (0.1, 0.1, 0.1, 0.1) for size 640x480
cv::Rect2d convert_to_fractional(const cv::Rect &rect, const cv::Size &view);
// Example: it converts from  (0.1, 0.1) -> (64, 48) for size 640x480
cv::Point2d convert_from_fractional(const cv::Point2d &p, const cv::Size &view);
// Example: it converts from  (0.1, 0.1, 0.1, 0.1) -> (64, 48, 64, 48) for size 640x480
cv::Rect2d convert_from_fractional(const cv::Rect2d &p, const cv::Size &view);

// CBOR output functions
cbor_item_t *rect_to_cbor(cv::Rect rect);
cbor_item_t *rect_to_cbor(cv::Rect2d rect);

// CBOR input functions
cv::Rect2d rect_from_cbor(cbor_item_t *item);
cv::Point2d point_from_cbor(cbor_item_t *item);

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
