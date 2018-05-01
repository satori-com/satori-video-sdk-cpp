# Satori Video SDK for C++ Concepts

[All Video SDK documentation](../README.md)

## Table of Contents
* [Publish-subscribe](#publish-subscribe)
* [Video SDK tools and examples](#video-sdk-tools-and-examples)
* [Image processing architecture](#image-processing-architecture)
    * [Streaming video](#streaming-video)
    * [Video publisher](#video-publisher)
    * [Video bot](#video-bot)
    * [Analysis bot](#analysis-bot)
    * [Metrics](#metrics)
    * [Bot configuration](#bot-configuration)
* [Build, deploy, and run](#build-deploy-and-run)
    * [Build](#build)
    * [Deploy](#deploy)
    * [Run](#run)
    * [Testing with execution modes](#testing-with-execution-modes)
* [OpenCV support](#opencv-support)
* [Logging and debugging](#logging-and-debugging)

## SDK Overview

The Satori Video SDK is a library of APIs for building bots that analyze compressed, streaming video in the form of messages.
The SDK subscribes to a channel containing streaming video messages, continuously receives these messages, decompresses
them, and converts them to individual image frames. To analyze these frames, you provide the SDK with an image
processing callback function that's invoked for each new frame. In this callback, you analyze the frames using your own
or 3rd-party libraries and publish results via an API call that uses the Satori publish-subscribe platform. This API
call can also publish debug and metrics messages to their own channels.

To help you communicate with the bot, the SDK also subscribes to a meta-data and configuration channel. Meta-data messages
contain codec and other information provided by the video source. Configuration messages contain parameters that control the
operation of the bot, so you can dynamically adjust the bot's behavior.

The SDK automatically connects to separate analysis, debug, and metrics channels. It also automatically subscribes to the
meta-data and configuration channel. The channel names are derived from the name of the input video channel you provide.
To publish a message, call `bot_message()` function and specify the channel using one of the `bot_message_kind`
`enum` constants.

The video bot build process statically links the API libraries and external dependencies into the bot executable. This
simplifies deployment, since the only file you need to deploy is the bot executable itself.

**Note:** This documentation refers to the SDK as a separate component, although it's actually built into the same
executable as your own code.

The following code snippet shows you the basic structure of a video bot:

```c++
namespace sv = satori::video;
namespace json = nlohmann::json;
namespace example_bot {
    /*
    * Defines a callback that the video bot SDK invokes when it finishes converting a frame.
    * The SDK passes in the global bot context and the converted frame
    */
    void process_image(sv::bot_context &context, const sv::image_frame &frame) {
        /*
        * Add your own code here!
        * Steps:
        * 1. Do some analysis of the image in the frame
        * 2. Create a JSON object containing the results of your analysis
        * 3. Call bot_message() to publish the message to the analysis channel
        /*
        sv::bot_message(context, sv::bot_message_kind::ANALYSIS, json::json json_message,
            sv::frame.frame_id);
    }
    /*
    * Defines a callback that the video bot SDK invokes when it receives a new message in the
    * control channel. The SDK passes in the global bot context and the received message
    */
    json process_command(sv::bot_context &context, json::json &message) {
    /*
    * Add your own code here!
    * Process the message and optionally store configuration information in the
    * bot context
    */
    }
}
/*
* main video bot program
int main(int argc, char *argv[]) {
  /*
  * Registers the bot with the API
  * Bot descriptor object contains the image format and the callback functions
  */
  sv::bot_register(sv::bot_descriptor{sv::image_pixel_format::BGR,
                                      &example_bot::process_image,
                                      &example_bot::process_command});
  /*
  * Starts the video bot main processing loop
  * bot_main sets up the SDK, starts ingesting video from the source specified in the
  * video bot command line configuration, and passes video frames to process_command().
  return sv::bot_main(argc, argv);
}
```

## Publish-subscribe
The SDK uses the [Satori](https://www.satori.com/docs/introduction/new-to-satori) publish-subscribe platform, which
provides reliable, high throughput I/O. It accepts real-time streaming video and can publish image processing results to
thousands of subscribers at once.

## Video SDK tools and examples
Besides the API library, the SDK provides command-line tools for video recording and playback. A public GitHub
repository contains example video bots that demonstrate how to use the SDK.

## Image processing architecture

Video bots are part of a larger image processing architecture:
* **Streaming video:** A video source such as a webcam sends out a video stream.
* **Video publisher:** A tool such as `satori_video_publisher` publishes the stream to an RTM channel.
* **Video bot:** A C++ bot you develop using the SDK.
* **Analysis bot:** A bot or bots that subscribe to the video bot analysis results channel.
* **Metrics:** The SDK has built-in support for the Prometheus monitoring platform.
* **Bot configuration:** Your video bot can receive configuration information from channel messages.

### Streaming video
The SDK supports these video sources:
* Webcam associated with a URL.
* macOS laptop camera (for testing)
* Video file. You can record video from a camera to a file with the `satori_video_recorder` utility and then use the
file as a source. The SDK supports the `.mp4`,`.mkv', and `.webm` video formats.

### Video publisher
The SDK command-line tool `satori_video_publisher` publishes video from a source to two Satori channels:
* **Video stream channel**: Messages containing compressed video
* **Video meta-data channel**: Codec and other video information

To learn more about the tool, see [Command-Line Utilities](reference.md#command-line-utilities).

### Video bot
Your C++ video bot analyzes image frames and publishes the results to the analysis channel.

### Analysis bot
Your video bot is often the first step in video processing. Other bots receive the messages in the
the analysis channel and do further processing. The motion detection bot described in the
[Satori Video SDK for C++ Tutorial](tutorial.md) is an example of this. The bot finds objects in the incoming
video stream and publishes their contours to the analysis channel. The bot assumes that another bot receives
the contours and tracks them over time to detect possible movement.

### Metrics
The SDK has built-in support for Prometheus based on the Prometheus Library for Modern C++. During
initialization, the SDK sets aside memory in the bot context for a Prometheus metrics registry. The SDK also sets up
a push server that your Prometheus server can scrape to collect metrics. The
[Satori Video SDK for C++ Tutorial](tutorial.md) demonstrates how to use Prometheus in a video bot.

### Bot configuration
To dynamically adjust the properties of your image processing function, you can publish a message containing the
properties to the control channel. For example, the motion detector bot described in
[Satori Video SDK for C++ Tutorial](tutorial.md) uses `process_command()` to set and update the feature size it uses
to detect contours in the image.

## Build, deploy, and run
The SDK uses a toolchain to based on `conan`, `cmake`, and GNU `make` to build video bots.

The examples in the SDK include Docker and
Kubernetes configurations for deploying and managing video bots, but you can use other tools as well. To learn more, see
[Satori Video SDK for C++: Build and Deploy a Video Bot](build_bot.md)

### Build
You must build video bots with the following toolchain:
* `conan`: Configures and installs dependencies based on the configuration of your build platform
* `cmake` to construct a build file
* GNU `make` to compile and build the bot.

### Deploy
The build process creates a C++ program that is statically linked to the SDK libraries and tools. The
built code has no outside dependencies, so you can run a bot on any macOS or Linux platform.

Because you use Satori channels for all input and output, bots are well-suited to running in Docker containers. The
video bot examples hosted in GitHub come with sample Docker files. In most cases, the Docker image for a video bot only
contains the bot itself. If your bot uses a initial config file, you may need to include that in your image as well.

Docker isn't required, and you can use other tools as well.
### Run

Because video bots are fully compatible with Docker, you can use Kubernetes to automate your video bot
deployment and operation. Like Docker, Kubernetes isn't required.

## OpenCV support
The SDK includes support for the **OpenCV** library of real-time computer vision functions.

Because OpenCV functions use the OpenCV `Mat` object, the SDK provides an additional OpenCV-oriented API
that replaces the default API. To learn more about this API, see [OpenCV API](reference.md#opencv-api).

## Testing with execution modes
The SDK provides two **execution modes** that let you control how your image processing callback function
processes frames:

* **LIVE:** The APIs drop frames instead of sending each frame to your callback.<br><br>
For example, suppose you're running in live mode, and the SDK invokes your callback with frame 1. While you're
processing this frame, the SDK decodes frames 2 through 6. When your callback returns, the SDK invokes your
callback with frame 7, dropping the other frames.<br><br>Live mode works with channel streams, files, and cameras.
**Note: Always use live mode when you run your bot in production.**

* **BATCH:** The SDK waits for your callback before sending the next frame instead of dropping frames. When the SDK
invokes your callback again, it sends the next sequential frame.<br><br>
For example, suppose you're running in batch mode, and the SDK invokes your callback with frame 1. While you're
processing frame 1, the SDK decodes frames 2 through 6. When your callback returns, the SDK invokes your
callback with *frame 2*. Your callback has the opportunity to process every frame in the stream.<br><br>
Batch mode only works with files. It's provided so you can test your bot during development. **Don't use batch mode
in production.**

## Logging and debugging
One of the channels that the SDK provides is intended for debug messages. To publish debug messages to it,
call `bot_message` and specify the `bot_message_kind::DEBUG` `enum` constant.

The SDK uses [Loguru](https://github.com/emilk/loguru) for logging. You can use it to do your own
logging, but you can also use any other logging framework you want.
