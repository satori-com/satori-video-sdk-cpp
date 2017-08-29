#pragma once

#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

#include "librtmvideo/rtmvideo.h"

namespace rtm {
namespace video {
namespace avutils {

std::string error_msg(const int av_error_code);

AVPixelFormat to_av_pixel_format(const image_pixel_format pixel_format);

}  // namespace avutils
}  // namespace video
}  // namespace rtm