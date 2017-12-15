#pragma once

#include <boost/optional.hpp>
#include <functional>
#include <gsl/gsl>
#include <memory>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

#include "data.h"
#include "satori_video.h"

namespace satori {
namespace video {
namespace avutils {

void init();

// Converts FFmpeg error code into human readable message.
std::string error_msg(int av_error_code);

// Converts image_pixel_format into FFmpeg's AVPixelFormat.
AVPixelFormat to_av_pixel_format(image_pixel_format pixel_format);

// Converts FFmpeg's AVPixelFormat into image_pixel_format.
image_pixel_format to_image_pixel_format(AVPixelFormat pixel_format);

// Creates FFmpeg's encoder context for encoder identified by encoder id.
std::shared_ptr<AVCodecContext> encoder_context(AVCodecID codec_id);

// Creates FFmpeg's decoder context for decoder identified by name.
std::shared_ptr<AVCodecContext> decoder_context(const std::string &codec_name,
                                                gsl::cstring_span<> extra_data);

std::shared_ptr<AVCodecContext> decoder_context(const AVCodec *decoder);

// Creates FFmpeg's AVFrame and allocates necessary fields.
std::shared_ptr<AVFrame> av_frame(int width, int height, int align,
                                  AVPixelFormat pixel_format);

// Creates FFmpeg's AVFrame.
std::shared_ptr<AVFrame> av_frame();

// Creates FFmpeg's AVPacket..
std::shared_ptr<AVPacket> av_packet();

// Creates FFmpeg's sws context based on source and destination frames.
// Sws context is used to scale images and convert pixel formats.
std::shared_ptr<SwsContext> sws_context(const std::shared_ptr<const AVFrame> &src_frame,
                                        const std::shared_ptr<const AVFrame> &dst_frame);

// Creates FFmpeg's sws context based on source and destination frames.
std::shared_ptr<SwsContext> sws_context(int src_width, int src_height,
                                        AVPixelFormat src_format, int dst_width,
                                        int dst_height, AVPixelFormat dst_format);

// Applies sws conversion to source frame and fills data of destination frame.
void sws_scale(const std::shared_ptr<SwsContext> &sws_context,
               const std::shared_ptr<const AVFrame> &src_frame,
               const std::shared_ptr<AVFrame> &dst_frame);

// Creates FFmpeg's format context to write data to files
std::shared_ptr<AVFormatContext> output_format_context(
    const std::string &format, const std::string &filename,
    const std::function<void(AVFormatContext *)> &file_cleaner);

std::shared_ptr<AVFormatContext> open_input_format_context(
    const std::string &url, AVInputFormat *forced_format = nullptr,
    AVDictionary *options = nullptr);
int find_best_video_stream(AVFormatContext *context, AVCodec **decoder_out);

// Copies image frame data to AVFrame
void copy_image_to_av_frame(const owned_image_frame &image,
                            const std::shared_ptr<AVFrame> &frame);

// Converts AVFrame to image frame
owned_image_frame to_image_frame(const std::shared_ptr<const AVFrame> &frame);

struct allocated_image {
  uint8_t *data[max_image_planes];
  int linesize[max_image_planes];
};

std::shared_ptr<allocated_image> allocate_image(int width, int height,
                                                image_pixel_format pixel_format);

boost::optional<image_size> parse_image_size(const std::string &str);
}  // namespace avutils
}  // namespace video
}  // namespace satori
