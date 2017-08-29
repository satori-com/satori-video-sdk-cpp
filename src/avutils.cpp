#include <stdexcept>

#include "avutils.h"

namespace rtm {
namespace video {
namespace avutils {

std::string error_msg(const int av_error_code) {
  char av_error[AV_ERROR_MAX_STRING_SIZE];
  av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, av_error_code);
  return std::string{av_error};
}

AVPixelFormat to_av_pixel_format(const image_pixel_format pixel_format) {
  switch (pixel_format) {
    case image_pixel_format::BGR:
      return AV_PIX_FMT_BGR24;
    case image_pixel_format::RGB0:
      return AV_PIX_FMT_RGB0;
    default:
      throw std::runtime_error{"Unsupported pixel format: " +
                               std::to_string((int)pixel_format)};
  }
}

}  // namespace avutils
}  // namespace video
}  // namespace rtm