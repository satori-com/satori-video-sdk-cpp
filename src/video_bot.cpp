#include "bot_environment.h"
#include "bot_instance.h"
#include "cbor_tools.h"
#include "metrics.h"
#include "stopwatch.h"

namespace satori {
namespace video {
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
  context.metrics.frame_processing_time_ms.Observe(s.millis());
  context.metrics.frames_processed_total.Increment();
}

std::vector<image_frame> drop_strategy_as_needed(const gsl::span<image_frame>& frames) {
  if (frames.size() > 1) {
    return {frames[frames.size() / 2 - 1], *(frames.end() - 1)};
  }
  return {frames.begin(), frames.end()};
}

std::vector<image_frame> drop_strategy_never(const gsl::span<image_frame>& frames) {
  return {frames.begin(), frames.end()};
}

using select_function_t =
    std::function<std::vector<image_frame>(const gsl::span<image_frame>& frames)>;

struct drop_strategy {
  select_function_t select_function;
  void update(cbor_item_t* config);
};

drop_strategy& get_drop_strategy() {
  static drop_strategy strategy{drop_strategy_as_needed};
  return strategy;
}

void drop_strategy::update(cbor_item_t* config) {
  if (cbor::map_has_str_value(config, "action", "configure")) {
    std::string drop_strategy = cbor::map_get_str(cbor::map_get(config, "body"),
                                                  "frame_drop_strategy", "as_needed");
    if (drop_strategy != "as_needed" && drop_strategy != "never") {
      ABORT() << "Unsupported drop strategy";
    }
    if (drop_strategy == "never") {
      select_function = drop_strategy_never;
      return;
    }
    select_function = drop_strategy_as_needed;
  }
}

bot_ctrl_callback_t to_drop_disabling_callback(const bot_ctrl_callback_t& callback) {
  return [callback](bot_context& context, cbor_item_t* message) {
    get_drop_strategy().update(message);
    if (callback) {
      return callback(context, message);
    }
    return (cbor_item_t*)nullptr;
  };
}

multiframe_bot_img_callback_t to_multiframe_bot_callback(
    const bot_img_callback_t& callback) {
  return [callback](bot_context& context, const gsl::span<image_frame>& frames) {
    CHECK(!frames.empty());
    auto selected_frames = get_drop_strategy().select_function(frames);
    for (const auto& f : selected_frames) {
      process_single_frame(context, callback, f);
    }
    context.metrics.frames_dropped_total.Increment(frames.size()
                                                   - selected_frames.size());
  };
}

}  // namespace

void bot_register(const bot_descriptor& bot) {
  multiframe_bot_register({bot.pixel_format, to_multiframe_bot_callback(bot.img_callback),
                           to_drop_disabling_callback(bot.ctrl_callback)});
}

int bot_main(int argc, char** argv) { return multiframe_bot_main(argc, argv); }

}  // namespace video
}  // namespace satori
