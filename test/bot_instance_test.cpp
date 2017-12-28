#define BOOST_TEST_MODULE BotInstanceTest
#include <boost/test/included/unit_test.hpp>

#include <json.hpp>

#include "bot_instance.h"
#include "streams/streams.h"

namespace sv = satori::video;

namespace {

void process_image(sv::bot_context &context, const gsl::span<sv::image_frame> &frame) {
  sv::bot_message(context, sv::bot_message_kind::DEBUG,
                  {{"dummy-debug-key", "dummy-debug-value"}}, frame[0].id);
  sv::bot_message(context, sv::bot_message_kind::ANALYSIS,
                  {{"dummy-analysis-key", "dummy-analysis-value"}}, frame[0].id);
}

nlohmann::json process_command(sv::bot_context & /*context*/,
                               const nlohmann::json &command) {
  auto &action = command["action"];

  if (action == "configure") {
    const std::string p = command["body"]["dummy-key"];
    BOOST_CHECK_EQUAL("dummy-value", p);

    return {{"dummy-configure-key", "dummy-configure-value"}};
  }

  if (action == "shutdown") {
    return {{"dummy-shutdown-key", "dummy-shutdown-value"}};
  }

  return nullptr;
}

}  // namespace

BOOST_AUTO_TEST_CASE(basic) {
  sv::multiframe_bot_descriptor descriptor;
  descriptor.pixel_format = sv::image_pixel_format::RGB0;
  descriptor.ctrl_callback = &::process_command;
  descriptor.img_callback = &::process_image;

  sv::bot_instance bot_instance{"dummy-bot-id", sv::execution_mode::BATCH, descriptor};
  nlohmann::json bot_config = R"({"dummy-key":"dummy-value"})"_json;

  bot_instance.configure(bot_config);

  sv::owned_image_packets frames;
  frames.push(sv::owned_image_frame{});

  std::vector<sv::bot_input> bot_input;
  bot_input.emplace_back(std::move(frames));

  sv::streams::publisher<sv::bot_input> bot_input_stream =
      sv::streams::publishers::of(std::move(bot_input));

  sv::streams::publisher<sv::bot_output> bot_output_stream =
      std::move(bot_input_stream) >> bot_instance.run_bot();

  struct bot_output_visitor : public boost::static_visitor<void> {
    void operator()(const sv::owned_image_metadata & /*metadata*/) {}
    void operator()(const sv::owned_image_frame & /*frame*/) {}
    void operator()(struct sv::bot_message &m) { messages.push_back(m); }
    std::vector<struct sv::bot_message> messages;
  };

  bot_output_visitor output_visitor;

  bot_output_stream->process(
      [&output_visitor](sv::bot_output &&o) { boost::apply_visitor(output_visitor, o); });

  BOOST_CHECK_EQUAL(4, output_visitor.messages.size());

  auto it = output_visitor.messages.begin();

  {
    const struct sv::bot_message &m = *it++;
    BOOST_CHECK_EQUAL((int)sv::bot_message_kind::DEBUG, (int)m.kind);
    BOOST_TEST("dummy-configure-value", m.data["dummy-configure-key"]);
  }

  {
    const struct sv::bot_message &m = *it++;
    BOOST_CHECK_EQUAL((int)sv::bot_message_kind::DEBUG, (int)m.kind);
    BOOST_TEST("dummy-debug-value", m.data["dummy-debug-key"]);
  }

  {
    const struct sv::bot_message &m = *it++;
    BOOST_CHECK_EQUAL((int)sv::bot_message_kind::ANALYSIS, (int)m.kind);
    BOOST_TEST("dummy-analysis-value", m.data["dummy-analysis-key"]);
  }

  {
    const struct sv::bot_message &m = *it++;
    BOOST_CHECK_EQUAL((int)sv::bot_message_kind::DEBUG, (int)m.kind);
    BOOST_TEST("dummy-shutdown-value", m.data["dummy-shutdown-key"]);
  }
}
