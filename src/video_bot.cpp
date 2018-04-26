#include "bot_environment.h"
#include "bot_instance.h"
#include "metrics.h"
#include "stopwatch.h"

namespace satori {
namespace video {
void bot_message(bot_context& context, const bot_message_kind kind,
                 nlohmann::json&& message, const frame_id& id) {
  CHECK(message.is_object()) << "Message must be an object: " << message;
  static_cast<bot_instance&>(context).queue_message(kind, std::move(message), id);
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
  void update(const nlohmann::json& config) {
    if (!config.is_object()) {
        LOG(4) << "config is not an object, drop strategy unaffected";
        return;
    }

    if (config.find("action") == config.end()) {
        LOG(4) << "no action in config, drop strategy unaffected";
        return;
    }

    if (config["action"] == "configure") {
      auto& body = config["body"];
      const std::string drop_strategy = body.find("frame_drop_strategy") != body.end()
                                            ? body["frame_drop_strategy"]
                                            : "as_needed";

      if (drop_strategy != "as_needed" && drop_strategy != "never") {
        ABORT() << "Unsupported drop strategy: " << config;
      }

      LOG(4) << "new drop strategy: " << drop_strategy;

      if (drop_strategy == "never") {
        select_function = drop_strategy_never;
        return;
      }
      select_function = drop_strategy_as_needed;
    }
  }

  select_function_t select_function;
};

drop_strategy& get_drop_strategy() {
  static drop_strategy strategy{drop_strategy_as_needed};
  return strategy;
}

bot_ctrl_callback_t to_drop_disabling_callback(const bot_ctrl_callback_t& callback) {
  return [callback](bot_context& context, const nlohmann::json& message) {
    get_drop_strategy().update(message);
    if (callback) {
      return callback(context, message);
    }
    return nlohmann::json(nullptr);
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
