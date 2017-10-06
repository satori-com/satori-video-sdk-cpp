#pragma once

#include <librtmvideo/video_bot.h>
#include <opencv2/opencv.hpp>

namespace satori {
namespace video {
namespace cvbot {

// Getting OpenCV image
// this cv::Mat object does not own the frame data, please perform deep clone
// if you need to use it outside of the frame callback runtime
cv::Mat get_image(const bot_context &context, const image_frame &frame);

cv::Rect2d convert_to_fractional(const cv::Rect &rect, const cv::Size &view);
cv::Point2d convert_from_fractional(const cv::Point2d &p, const cv::Size &view);
cv::Rect2d convert_from_fractional(const cv::Rect2d &p, const cv::Size &view);

// CBOR output functions
cbor_item_t *rect_to_cbor(cv::Rect rect);
cbor_item_t *rect_to_cbor(cv::Rect2d rect);

// CBOR input functions
cv::Rect2d rect_from_cbor(cbor_item_t *item);
cv::Point2d point_from_cbor(cbor_item_t *item);

}
}
}