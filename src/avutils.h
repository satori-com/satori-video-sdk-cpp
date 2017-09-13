#pragma once

#include <functional>
#include <memory>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

#include "librtmvideo/data.h"
#include "librtmvideo/rtmvideo.h"

namespace rtm {
namespace video {
namespace avutils {

void init();

// Converts FFmpeg error code into human readable message.
std::string error_msg(const int av_error_code);

// Converts image_pixel_format into FFmpeg's AVPixelFormat.
AVPixelFormat to_av_pixel_format(const image_pixel_format pixel_format);

// Creates FFmpeg's encoder context for encoder identified by encoder id.
std::shared_ptr<AVCodecContext> encoder_context(const AVCodecID codec_id);

// Creates FFmpeg's AVFrame and allocates necessary fields.
std::shared_ptr<AVFrame> av_frame(int width, int height, int align,
                                  AVPixelFormat pixel_format);

// Creates FFmpeg's sws context based on source and destination frames.
// Sws context is used to scale images and convert pixel formats.
std::shared_ptr<SwsContext> sws_context(std::shared_ptr<const AVFrame> src_frame,
                                        std::shared_ptr<const AVFrame> dst_frame);

// Applies sws conversion to source frame and fills data of destination frame.
void sws_scale(std::shared_ptr<SwsContext> sws_context,
               std::shared_ptr<const AVFrame> src_frame,
               std::shared_ptr<AVFrame> dst_frame);

// Creates FFmpeg's format context to write data to files
std::shared_ptr<AVFormatContext> format_context(
    const std::string &format, const std::string &filename,
    std::function<void(AVFormatContext *)> file_cleaner);

// Copies image frame data to AVFrame
void copy_image_to_av_frame(const owned_image_frame &image,
                            std::shared_ptr<AVFrame> frame);
}  // namespace avutils
}  // namespace video
}  // namespace rtm
