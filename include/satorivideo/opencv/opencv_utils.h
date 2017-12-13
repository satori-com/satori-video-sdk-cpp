#pragma once

#include <cbor.h>
#include <satorivideo/video_bot.h>
#include <functional>
#include <iostream>
#include <opencv2/opencv.hpp>

namespace satori {
namespace video {
namespace opencv {

cv::Rect2d convert_to_fractional(const cv::Rect &rect, const cv::Size &view);
cv::Point2d convert_from_fractional(const cv::Point2d &p, const cv::Size &view);
cv::Rect2d convert_from_fractional(const cv::Rect2d &p, const cv::Size &view);

// CBOR output functions
cbor_item_t *rect_to_cbor(cv::Rect rect);
cbor_item_t *rect_to_cbor(cv::Rect2d rect);

// CBOR input functions
cv::Rect2d rect_from_cbor(cbor_item_t *item);
cv::Point2d point_from_cbor(cbor_item_t *item);

// Debugging functions
void log_image(const cv::Mat &image);

}  // namespace opencv
}  // namespace video
}  // namespace satori
