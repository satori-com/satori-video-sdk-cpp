# Satori Video SDK for C++ Concepts

[All Video SDK documentation](../README.md)

## SDK Overview

The Satori Video SDK processes compressed streaming video. It provides a bot framework that decodes video into frames.
The framework passes the frames to image processing code that you provide. In turn, your code analyzes or
transforms the video data. A C++ API lets you publish image processing results, debug messages, and metrics from the
bot. The API also lets you receive messages containing configuration information.

The SDK uses the [Satori](https://www.satori.com/docs/introduction/new-to-satori) publish/subscribe platform, which
provides reliable, high throughput I/O. It accepts real-time streaming video and can
publish image processing results to thousands of subscribers at once.

Besides the API, the SDK includes command-line utilities for video recording and playback, and code examples that
demonstrate how to use the SDK for common video-processing tasks.

## Satori Video architecture
The Satori Video SDK architecture is a bot framework that calls SDK library functions to receive and decode video from
[Satori channels](https://www.satori.com/docs/using-satori/overview). Your code communicates with the bot framework
using the SDK API to register itself and start a **main event loop**.
During the lifetime of the loop, the framework decodes streaming video into frames and passes them to an image
processing callback you provide.

In this function, you can analyze or transform video frames. Using the API, you can
publish analysis, metrics, and debug messages to one of the channels that the framework automatically provides.

![Diagram showing the Satori video architecture](images/video_sdk_schematic.png "Satori Video Architecture")
## Satori Video lifecycle
The Satori Video SDK has this life cycle:
* You provide a video source
* An SDK utility publishes a video stream
* You run the video bot
* The bot runs the main event loop
* You process video and publish results

### You provide a video source
You can provide streaming video from these sources:
* Webcam associated with a URL.
* macOS laptop camera (for testing)
* Video file. You can record video from a camera to a file with the `satori_video_recorder` utility and then use the
file as a source. The Video SDK supports the `.mp4`,`.mkv', and `.webm` video formats.

### An SDK utility publishes a video stream
The Satori Video SDK command-line utility `satori_video_publisher` publishes video from a source to two Satori channels:

* **Video stream channel**: Messages containing compressed video.
* **Video metadata channel**: Codec and other video information.

To learn more about the utility, see [Command-Line Utilities](reference.md#command-line-utilities).

### You run the video bot
When you run the video bot, it executes the `main()` function. This function calls `bot_register()`
to pass video information, the name of your image processing callback, and the name of your control message callback to
the bot framework. To start image processing, you call the API function `bot_main()`, which starts the main event loop.

### Run main event loop
During initialization, the framework invokes the configuration callback, passing in configuration parameters you
pass to the bot on the command line (see [Configuration message callback](reference.md#configuration-message-callback).

The main event loop runs in the framework. It receives messages from the video stream channel, decodes them, re-assembles
them into video frames, and invokes the image processing callback with the frames as a parameter. When your callback
returns to the SDK, the loop does its next iteration.

### Process video
During each iteration of the main event loop, the SDK passes one or more video frames to your image processing callback.
In this function, you can analyze and transform frames. The SDK automatically provides your bot with an analysis
channel to which you can publish results. The SDK caches analysis messages and publishes all of them once during each
loop iteration.

## Video bot environment
Video bots are C++ programs that are statically linked to the Video SDK libraries and tools when you build them. The
built code has no outside dependencies, so you can run bot on any macOS or Linux platform.

Because you use Satori channels for all input and output to a bot, including control and configuration, bots are
well-suited to running in Docker containers using a cloud environment such as Kubernetes. The video bot examples hosted
in GitHub come with sample Dockerfile and Kubernetes files.

## OpenCV support
The Video SDK includes support for the **OpenCV** library of real-time computer vision functions.

Because OpenCV functions use the OpenCV `Mat` object, the Video SDK provides an additional OpenCV-oriented API
that replaces the default API. To learn more about this API, see [OpenCV API](reference.md#opencv-api).

## Frame identifier
The framework assigns a **frame identifier** to each video frame it decodes from the input stream. This
identifier is a sequence number rather than a time code, and it helps you manage and annotate frames.

The identifier is stored as two 64-bit integers, each of which contains the same sequence number. In the C++ API,
the identifier is represented by the `frame_id` type struct that has two fields, `id1` and `id2`.

The `bot_message()` API function accepts a `frame_id` value as an optional parameter, which becomes part of the
published message. If you want mark the message as applying to a range of frames, use `id1` to refer to the first
frame in the range, and `id2` to refer to the last frame.

## Execution mode
The framework provides two **execution modes** that let you control how your image processing callback
processes frames:

* **LIVE:** The framework drops frames instead of sending each frame to your callback.<br><br>
For example, suppose you're running in live mode, and the framework invokes your callback with frame 1. While you're
processing this frame, the framework decodes frames 2 through 6. When your callback returns, the framework invokes your
callback with frame 7, dropping the other frames.<br><br>Live mode works with channel streams, files, and cameras.
**Note: Always use live mode when you run your bot in production.**

* **BATCH:** The framework waits for your callback instead of dropping frames. The framework doesn't send the next
frame until your callback returns. When the framework invokes your callback again, it sends the next sequential frame.<br><br>
For example, suppose you're running in batch mode, and the framework invokes your callback with frame 1. While you're
processing frame 1, the framework decodes frames 2 through 6. When your callback returns, the framework invokes your
callback with *frame 2*. Your callback has the opportunity to process every frame in the stream.<br><br>
Batch mode only works with files. It's provided so you can test your bot during development. **Don't use batch mode
in production.**

## Bot context
The context for a bot is a C++ variable that communicates settings to the framework and persists data across iterations
of the main event loop. Use the `instance_data` pointer in the variable to point to your own global information. The
framework passes the context as a parameter whenever it invokes your image processing or control callback.

## Logging and debugging
The bot framework provides a debug channel name based on the video channel name. To publish debug messages to it,
call `bot_message`.

To learn about SDK channels, see [SDK channel names](reference.md#sdk-channel-names).

The bot framework uses [Loguru](https://github.com/emilk/loguru) for logging. You can use it to do your own
logging, but you can also use any other logging framework you want.

## Prometheus support
Support for the Prometheus metrics library is built into the framework. The bot context contains a variable you
can use to store a Prometheus `Registry` object. Use of the library is optional.

See also
[Video bot command-line parameters](reference.md#video-bot-command-line-parameters).

## Docker support
Video bots are fully compatible with Docker. During the video bot build process, the SDK statically links
all of the SDK libraries and prerequisites with the bot framework and your code. In most cases, the
Docker image for a video bot only contains the bot itself. If your bot uses a initial config file, you
may need to include that in your image as well.

## Kubernetes support
Because video bots are fully compatible with Docker, you can use Kubernetes to automate your video bot
deployment and operation.

## Supported video data formats
The framework supports the following frameworks:<br>
• MPEG-4 Part 10 (H.264) and MPEG-4 Part 2 compressed video. File extension is `.mp4`.<br>
• Matroska Multimedia Container (**MKV**). File extension is `.mkv`.<br>
• WebM. File extension is `.webm`.<br>


