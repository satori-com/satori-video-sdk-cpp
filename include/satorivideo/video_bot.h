// Video bot API.
// To run a video bot:
// - Call bot_register to register the bot with Satori
// - Call bot_main to start the video bot main processing event loop.
//
// Example:
// int main(int argc, char *argv[]) {
//   satori::video::bot_register(bot_descriptor{image_pixel_format::BGR,
//                                              &transcoder::process_image});
//   return satori::video::bot_main(argc, argv);
// }
//
// For more information, see the definitions of bot_register, bot_descriptor, and bot_main
//

#pragma once

#include <cstdint>
#include <functional>

#include "base.h"

// libcbor declarations
// libcbor provides an API for CBOR-formatted channel messages, such as streaming video.
struct cbor_item_t;

// prometheus-cpp declarations
// The Satori video bot API uses the Prometheus API for telemetry.
namespace prometheus {
class Registry;
}

namespace satori {
namespace video {

// Every image belongs to a certain time interval, setting values wider
// makes annotation applicable to multiple video frames
//
// Time-independent frame identifier provided by the video bot
//
EXPORT struct frame_id {
  int64_t i1;
  int64_t i2;
};

// Provides for as many as 4 separate planes for an image.
// Packed RGB or YUV use plane 0.
constexpr uint8_t MAX_IMAGE_PLANES = 4;

// If the video stream uses a packed pixel format such as packed RGB or packed YUV,
// then it has only a single plane. All of the data is stored in plane_data[0].
// An image in planar pixel format such as planar YUV or HSV stores each component as a separate array (separate plane).
// For example, with the planar YUV format, Y is plane_data[0], U is plane_data[1] and V is
// plane_data[2].
// A stride is a plane size with alignment.
EXPORT struct image_frame {
  frame_id id;
  const uint8_t *plane_data[MAX_IMAGE_PLANES];
};

//
// Describes the format of all images in the channel: image width, height, and number of planes per frame
//
EXPORT struct image_metadata {
  uint16_t width;
  uint16_t height;
  uint32_t plane_strides[MAX_IMAGE_PLANES];
};

// Indicators for controlling how your bot should react if the main event loop is running behind the rate of incoming
// video messages.
// `live`: Drop frames in order to keep up with stream
// `batch`: Allow analysis task to process every frame
EXPORT enum class execution_mode { LIVE = 1, BATCH = 2 };

//
// Persists global state for the main event loop.
// During the first iteration of the loop, bot_context is null.
// Use `bot_context` instead of global storage.
//
EXPORT struct bot_context {
  // Global values
  void *instance_data{nullptr};
  // Frame size information
  const image_metadata *frame_metadata;
  // Current mode (batch or live)
  const execution_mode mode;
  // Prometheus registry information
  prometheus::Registry &metrics_registry;
};

//
// Image handler callback function
//
// Defines the signature of the function that processes incoming images.
//
// When you register a video bot, you pass a parameter that includes a pointer to your image analysis function.
// For each iteration of the main event loop, the bot calls this function with the following parameters:
// - context: A bot_context containing the current persistent context
// - frame: An image_frame containing the next frame assembled from the video stream
using bot_img_callback_t =
    std::function<void(bot_context &context, const image_frame &frame)>;

//
// Control channel callback function
//
// Defines the signature of the function that processes messages from the control channel.
//
// When you register a video bot, you pass a parameter that includes a pointer to your control channel function.
// The video bot automatically subscribes to the control channel, and it invokes the function whenever a new control
// message arrives. Use these messages to configure the video bot on startup and control its operation.
//
// This function is always invoked during initialization, even if the bot doesn't receive a message.
//
// Although control channel messages can have any format, you should construct them as JSON objects with this format:
// { "action" : "configure",
//   "body" : {
//     <configuration_parameters
//   }
// }
//
using bot_ctrl_callback_t =
    std::function<cbor_item_t *(bot_context &context, cbor_item_t *message)>;

//
// Indicators for specifying the format of each pixel in the incoming stream
//
enum class image_pixel_format { RGB0 = 1, BGR = 2 };

//
// Control structure that communicates your settings to the video bot
//
struct bot_descriptor {
  // Pixel format, like RGB0, BGR, etc.
  image_pixel_format pixel_format;

  // Function to invoke whenever the video bot assembles a new frame from the incoming video stream
  bot_img_callback_t img_callback;

  // Function to invoke whenever the video bot receives a control channel message. The bot always invokes this
  // function during bot initialization, even if it doesn't receive a messsage
  bot_ctrl_callback_t ctrl_callback;
};

// Indicators that specify which channel to use for a message
EXPORT enum class bot_message_kind { ANALYSIS = 1, DEBUG = 2, CONTROL = 3 };

// Publishes a message to one of the subchannels for this bot.
//
// All of the data you send is aggregated and and published as a single message at the end of each main event loop
// iteration (after the image callback function returns).
//
// The id parameter specifies the frame associated with the message. By default, the id value is frame_id{0,0}, which
// indicates that the message is for the current frame.
//
// See rtmvideo.md to learn more about the channels provides by the Video Stream platform.
EXPORT void bot_message(bot_context &context, bot_message_kind kind, cbor_item_t *message,
                        const frame_id &id = frame_id{0, 0});

// Registers the bot, including your settings, with RTM
// Call this function before you start the main event loop.
EXPORT void bot_register(const bot_descriptor &bot);

// Starts the bot (launches the main event loop).
// Remember to register your bot before calling this function
EXPORT int bot_main(int argc, char *argv[]);

}  // namespace video
}  // namespace satori
