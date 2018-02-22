#pragma once

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavutil/frame.h>
}

#include "satorivideo/video_bot.h"

namespace satori {
namespace video {

class av_filter {
 public:
  av_filter(const std::string &description, const AVFrame &sample_input_frame,
            const AVRational &time_base, image_pixel_format output_pixel_format);

  ~av_filter();

  void feed(const AVFrame &in);
  bool try_retrieve(AVFrame &out);

 private:
  const image_pixel_format _output_pixel_format;
  AVFilterInOut *_outputs{nullptr};
  AVFilterInOut *_inputs{nullptr};
  AVFilterGraph *_filter_graph{nullptr};
  AVFilterContext *_buffer_sink_ctx{nullptr};
  AVFilterContext *_buffer_src_ctx{nullptr};
};

}  // namespace video
}  // namespace satori
