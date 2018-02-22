// Code here is mostly based on filtering_video.c example from FFmpeg
#include "av_filter.h"

#include <sstream>

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
}

#include "avutils.h"
#include "logging.h"

namespace satori {
namespace video {

av_filter::av_filter(const std::string &description, const AVFrame &sample_input_frame,
                     const AVRational &time_base,
                     const image_pixel_format output_pixel_format)
    : _output_pixel_format{output_pixel_format} {
  avutils::init();

  const AVFilter *buffer_src = avfilter_get_by_name("buffer");
  const AVFilter *buffer_sink = avfilter_get_by_name("buffersink");
  CHECK_NOTNULL(buffer_src) << "couldn't find filter buffer";
  CHECK_NOTNULL(buffer_sink) << "couldn't find filter buffersink";

  _outputs = avfilter_inout_alloc();
  _inputs = avfilter_inout_alloc();
  _filter_graph = avfilter_graph_alloc();
  CHECK_NOTNULL(_outputs);
  CHECK_NOTNULL(_inputs);
  CHECK_NOTNULL(_filter_graph);

  std::ostringstream args_builder;
  args_builder << "video_size=" << sample_input_frame.width << "x"
               << sample_input_frame.height;
  args_builder << ":pix_fmt=" << sample_input_frame.format;
  if (time_base.num > 0) {
    args_builder << ":time_base=" << time_base.num << "/" << time_base.den;
  } else {
    args_builder << ":time_base=1/1";
  }
  args_builder << ":pixel_aspect=" << sample_input_frame.sample_aspect_ratio.num << "/"
               << sample_input_frame.sample_aspect_ratio.den;

  const std::string args = args_builder.str();
  LOG(INFO) << "filter buffer source args: " << args;

  int ret = avfilter_graph_create_filter(&_buffer_src_ctx, buffer_src, "in", args.c_str(),
                                         nullptr, _filter_graph);
  CHECK_GE(ret, 0) << "Cannot create buffer source: " << avutils::error_msg(ret);
  CHECK_NOTNULL(_buffer_src_ctx);

  /* buffer video sink: to terminate the filter chain. */
  ret = avfilter_graph_create_filter(&_buffer_sink_ctx, buffer_sink, "out", nullptr,
                                     nullptr, _filter_graph);
  CHECK_GE(ret, 0) << "Cannot create buffer sink: " << avutils::error_msg(ret);
  CHECK_NOTNULL(_buffer_sink_ctx);

  AVPixelFormat pix_fmts[]{avutils::to_av_pixel_format(_output_pixel_format),
                           AV_PIX_FMT_NONE};
  ret = av_opt_set_int_list(_buffer_sink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE,
                            AV_OPT_SEARCH_CHILDREN);
  CHECK_GE(ret, 0) << "Cannot set output pixel format: " << avutils::error_msg(ret);

  /*
   * Set the endpoints for the filter graph. The filter_graph will
   * be linked to the graph described by filters_descr.
   */

  /*
   * The buffer source output must be connected to the input pad of
   * the first filter described by filters_descr; since the first
   * filter input label is not specified, it is set to "in" by
   * default.
   */
  _outputs->name = av_strdup("in");
  _outputs->filter_ctx = _buffer_src_ctx;
  _outputs->pad_idx = 0;
  _outputs->next = nullptr;

  /*
   * The buffer sink input must be connected to the output pad of
   * the last filter described by filters_descr; since the last
   * filter output label is not specified, it is set to "out" by
   * default.
   */
  _inputs->name = av_strdup("out");
  _inputs->filter_ctx = _buffer_sink_ctx;
  _inputs->pad_idx = 0;
  _inputs->next = nullptr;

  ret = avfilter_graph_parse_ptr(_filter_graph, description.c_str(), &_inputs, &_outputs,
                                 nullptr);
  CHECK_GE(ret, 0) << "Cannot parse filter description \"" << description
                   << "\": " << avutils::error_msg(ret);

  ret = avfilter_graph_config(_filter_graph, nullptr);
  CHECK_GE(ret, 0) << "Cannot configure filter graph: " << avutils::error_msg(ret);
}

av_filter::~av_filter() {
  avfilter_free(_buffer_src_ctx);
  avfilter_free(_buffer_sink_ctx);
  avfilter_inout_free(&_inputs);
  avfilter_inout_free(&_outputs);
  avfilter_graph_free(&_filter_graph);
}

void av_filter::feed(const AVFrame &in) {
  /* push the decoded frame into the filtergraph */
  int ret = av_buffersrc_write_frame(_buffer_src_ctx, &in);
  CHECK_GE(ret, 0) << "Error while feeding the filtergraph: " << avutils::error_msg(ret);
}

bool av_filter::try_retrieve(AVFrame &out) {
  /* pull filtered frames from the filtergraph */
  int ret = av_buffersink_get_frame(_buffer_sink_ctx, &out);
  if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
    return false;
  }
  CHECK_GE(ret, 0) << "Error getting frame out of filtergraph: "
                   << avutils::error_msg(ret);
  return true;
}

}  // namespace video
}  // namespace satori
