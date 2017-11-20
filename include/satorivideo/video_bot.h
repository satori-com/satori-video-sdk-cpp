//
// Video bot API.
// The video bot API provides access to the Satori Video bot framework from a C++ bot.
// The framework decodes incoming video streams and passes the resulting video frames to a callback function that
// you define. In this function, you can analyze, transform, or replicate the frames. You can then publish the
// results to an RTM channel.
//
// A video bot can also receive messages from an RTM channel called a **control channel**. Use this channel to
// configure the bot.
//
// All video bots must call the following functions:
// - `bot_register`: Registers the bot with the Satori Video bot framework
// - `bot_main`: Starts the main event loop in the framework
//
// The main event loop decodes the video stream to produce video frames that it passes to your image processing
// callback function.
//
// Example:
// ```c++
// int main(int argc, char *argv[]) {
//   satori::video::bot_register(bot_descriptor{image_pixel_format::BGR,
//                                              &transcoder::process_image});
//   return satori::video::bot_main(argc, argv);
// }
// ```
// For more information, see the definitions of bot_register, bot_descriptor, and bot_main
//

#pragma once

#include <cstdint>
#include <functional>

#include "base.h"

//
// libcbor declarations
// libcbor provides an API for CBOR-formatted messages, such as streaming video.
//
struct cbor_item_t;

//
// prometheus-cpp declarations
// The Satori video bot API uses the Prometheus API for telemetry.
//
namespace prometheus {
class Registry;
}

namespace satori {
namespace video {

//
//
// Frame identifier that's not time-related. The identifier is the sequence number of the frame in the stream.
//
// If i1 == i2, the id refers to a single frame.
// If i1 < i2, the id refers to a range of frames, starting with frame i1 and ending with frame i2.
//
// When the bot framework invokes the image callback function, it passes an `image_frame` that contains a `frame_id`
// and the image data. In this case, i1 == i2 and the id is for a single frame.
//
// The `bot_message` function has an `id` parameter in which you can pass a `frame_id` value that identifies the
// frame or frames associated with the message.
//
EXPORT struct frame_id {
  int64_t i1;
  int64_t i2;
};

//
// Provides for as many as 4 separate data planes for an image.
// Packed formats such as packed RGB and packed YUV only use plane 0.
//
constexpr uint8_t MAX_IMAGE_PLANES = 4;

//
// If the video stream uses a packed pixel format such as packed RGB or packed YUV,
// then it has only a single data plane. All of the data is stored in `plane_data[0]`.
// An image in planar pixel format such as planar YUV or HSV stores each component as a separate array (separate plane).
// For example, with the planar YUV format, Y is `plane_data[0]`, U is `plane_data[1]` and V is
// `plane_data[2]`.
//
EXPORT struct image_frame {
  frame_id id;
  const uint8_t *plane_data[MAX_IMAGE_PLANES];
};

//
// Describes the contents of each frame: image width, height, and number of plane strides.
// A plane stride is an aligned `plane_data`.
//
EXPORT struct image_metadata {
  uint16_t width;
  uint16_t height;
  uint32_t plane_strides[MAX_IMAGE_PLANES];
};

//
// Indicators for controlling how the main event loop decodes frames and hands them off to your image callback.
//
// - `live`: Live video stream mode. The bot framework sends frames to the image callback based on the
// incoming frame rate. If the image callback lags behind, the bot framework drops frames to stay in sync with the frame
// rate. This mode is available for RTM channel streams, camera input, and files.
// **Use live mode for bots running in production.**
//
// -`batch`: Batch (test) input mode. The bot framework waits for the image callback to return before sending it another
// frame, so no frames are dropped. This mode is only available for files.
// **Only use `batch` mode for testing.**
//
EXPORT enum class execution_mode { LIVE = 1, BATCH = 2 };

//
// Persists global state for your bot.
// During the first iteration of the loop, bot_context is null.
// **Use `bot_context` instead of global variables.**
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
// Image frame processing callback function
//
// Defines the signature of the function that processes incoming frames.
//
// When you register a video bot, you pass a parameter that includes a pointer to an image processing function.
// For each frame, the bot framework calls this function with the following parameters:
// - context: A bot_context containing the current persistent context
// - frame: An image_frame containing the next frame assembled from the video stream
//
// **NOTE:** In `live` execution mode, the bot framework may drop incoming frames in order to stay in sync with
// the incoming frame rate.
//
using bot_img_callback_t =
    std::function<void(bot_context &context, const image_frame &frame)>;

//
// Control channel callback function
//
// Defines the signature of the function that processes messages from the control channel.
//
// The bot framework automatically subscribes to a control channel associated with your bot.
// When you register your video bot, you pass a parameter that includes a pointer to a function that the framework
// invokes when it receives a control channel message. You can use these messages to configure the video bot on startup
// and control its operation.
//
// The framework always invokes this function during initialization. If there's no message in the channel at that
// point, `message` is null.
//
// Construct messages JSON objects with this format:
// { "action" : "configure",
//   "body" : {
//     <optional configuration parameters>
//   }
// }
//
using bot_ctrl_callback_t =
    std::function<cbor_item_t *(bot_context &context, cbor_item_t *message)>;

//
// Indicators for specifying the format of incoming images
//
enum class image_pixel_format { RGB0 = 1, BGR = 2 };

//
// Control structure that communicates settings to the video bot framework
//
struct bot_descriptor {
  // Pixel format, like RGB0, BGR, etc.
  image_pixel_format pixel_format;

  // Function to invoke whenever the bot framework assembles a new frame from the incoming video stream
  bot_img_callback_t img_callback;

  // Function to invoke whenever the bot framework receives a control channel message. The bot always invokes this
  // function during bot initialization, even if it doesn't receive a messsage
  bot_ctrl_callback_t ctrl_callback;
};

// Indicators that specify which channel to use for a message
EXPORT enum class bot_message_kind { ANALYSIS = 1, DEBUG = 2, CONTROL = 3 };

//
// Publishes a message to one of the subchannels for this bot.
//
// All of the data you send is aggregated and published as a single message at the end of each main event loop
// iteration (after the image callback function returns).
//
// The id parameter specifies the frame or frames associated with the message. By default, the id value is
// frame_id{0,0}, which indicates that the message is for the current frame. Set `frame_id.id1` == `frame_id.id2` to
// refer to a single frame, or for a range of frames, set `frame_id.id1` to the first frame in the range, and
// `frame_id.id2` to the last frame in the range.
//
EXPORT void bot_message(bot_context &context, bot_message_kind kind, cbor_item_t *message,
                        const frame_id &id = frame_id{0, 0});

//
// Registers the bot, including your settings
// Call this function before you start the main event loop.
//
EXPORT void bot_register(const bot_descriptor &bot);

//
// Starts the bot (launches the main event loop).
// Remember to register your bot before calling this function
//
EXPORT int bot_main(int argc, char *argv[]);

}  // namespace video
}  // namespace satori
