#include "bot_environment.h"
#include "bot_instance.h"
#include "metrics.h"
#include "stopwatch.h"

namespace satori {
namespace video {
namespace {
auto& frames_processed_total = prometheus::BuildCounter()
                                   .Name("frames_processed_total")
                                   .Register(metrics_registry())
                                   .Add({});
auto& frames_dropped_total = prometheus::BuildCounter()
                                 .Name("frames_dropped_total")
                                 .Register(metrics_registry())
                                 .Add({});
auto& processing_times_millis =
    prometheus::BuildHistogram()
        .Name("frame_processing_times_millis")
        .Register(metrics_registry())
        .Add({}, std::vector<double>{0,  1,  2,  5,  10,  15,  20,  25,  30,  40, 50,
                                     60, 70, 80, 90, 100, 200, 300, 400, 500, 750});

}  // namespace

void bot_message(bot_context& context, const bot_message_kind kind, cbor_item_t* message,
                 const frame_id& id) {
  CHECK(cbor_map_is_indefinite(message)) << "Message must be indefinite map";
  static_cast<bot_instance&>(context).queue_message(kind, message, id);
}

void multiframe_bot_register(const multiframe_bot_descriptor& bot) {
  bot_environment::instance().register_bot(bot);
}

int multiframe_bot_main(int argc, char* argv[]) {
  return bot_environment::instance().main(argc, argv);
}

namespace {
void process_single_frame(bot_context& context, const bot_img_callback_t& callback,
                          const image_frame frame) {
  stopwatch<> s;
  static_cast<bot_instance&>(context).set_current_frame_id(frame.id);
  callback(context, frame);
  static_cast<bot_instance&>(context).set_current_frame_id({0, 0});
  processing_times_millis.Observe(s.millis());
  frames_processed_total.Increment();
}
multiframe_bot_img_callback_t to_multiframe_bot_callback(
    const bot_img_callback_t& callback) {
  return [callback](bot_context& context, const gsl::span<image_frame>& frames) {
    size_t processed = 1;
    CHECK(!frames.empty());

    if (frames.size() > 1) {
      process_single_frame(context, callback, frames[frames.size() / 2 - 1]);
      process_single_frame(context, callback, *(frames.end() - 1));
      processed++;
    } else {
      process_single_frame(context, callback, frames[0]);
    }

    frames_dropped_total.Increment(frames.size() - processed);
  };
}

}  // namespace

void bot_register(const bot_descriptor& bot) {
  multiframe_bot_register({image_pixel_format::BGR,
                           to_multiframe_bot_callback(bot.img_callback),
                           bot.ctrl_callback});
}

int bot_main(int argc, char** argv) { return multiframe_bot_main(argc, argv); }

}  // namespace video
}  // namespace satori
