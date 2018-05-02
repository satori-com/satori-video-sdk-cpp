# Satori Video SDK for C++ Reference

[All Video SDK documentation](../README.md)

## Table of contents
* [SDK API](#sdk-api)
    * [Base includes](#base-includes)
    * [OpenCV includes](#opencv-includes)
    * [Constants](#constants)
    * [Function types](#function-types)
    * [Structs](#structs)
    * [Enums](#enums)
    * [Functions](#functions)
* [OpenCV-compatible API](#opencv-compatible-api)
    * [OpenCV types](#opencv-types)
    * [OpenCV structs](#opencv-structs)
    * [OpenCV functions](#opencv-functions)
* [Template video bots](#template-video-bots)
* [Supported video data formats](#supported-video-data-formats)
* [SDK channel names](#sdk-channel-names)
* [Video bot command-line parameters](#video-bot-command-line-parameters)
* [Command-line tools](#command-line-tools)
    * [satori_video_publisher](#satori-video-publisher)
    * [satori_video_player](#satori-video-player)
    * [satori_video_recorder](#satori-video-recorder)

## SDK API

### Base includes
The base API for the video SDK is declared in the header file [`video_bot.h`](../include/satorivideo/video_bot.h).

The API also uses the following libraries:

| Library                                                                             | Header file |
|-------------------------------------------------------------------------------------|-------------|
| [JSON for Modern C++](https://nlohmann.github.io/json/index.html)                   | json.hpp    |
| [Loguru Header-only C++ Logging Library](https://emilk.github.io/loguru/index.html) | loguru.hpp  |
| [Prometheus Library for Modern C++](https://github.com/jupp0r/prometheus-cpp)       | (several)   |

The bot build process uses `conan` to install these files.

### OpenCV includes
If you want to use the OpenCV image processing library in your video bot, you also need the following libraries:

| File                                                             | Description                |
|------------------------------------------------------------------|----------------------------|
| [`opencv_bot.h`](../include/satorivideo/opencv/opencv_bot.h)     | OpenCV video bot API       |
| [`opencv_utils.h`](../include/satorivideo/opencv/opencv_utils.h) | OpenCV video bot utilities |
| `opencv.hpp` for OpenCV2Z3221`221` (installed during the build)  | OpenCV API                 |

The bot build process uses `conan` to install these files.

### Constants

#### `MAX_IMAGE_PLANES`
| Variable/type name | Type     | Value       |Description                |
|--------------------| :------: | :---------: | :-------------------------|
| `MAX_IMAGE_PLANES` | `uint8_t`| 4           | Max number of data planes |

Unsigned integer set to 4. Defines the maximum number of data planes a frame can contain.

### Function types

#### `bot_img_callback_t`
Function type for an image processing callback, as an alias defined by the following function template:<br>
`using bot_img_callback_t = std::function<void(bot_context &context, const image_frame &frame)>;`

| Parameter     | Type            | Description                             |
|---------------|-----------------|-----------------------------------------|
| `context`     | `bot_context`   | (Read-write) Global context for the bot |
| `image_frame` | `image_frame`   | (Read-only) Video frame                 |

returns `void`

After it decodes a video frame from the incoming video stream, the SDK invokes a function you define and passes it
the frame. Use this function to process the video frame.

Create a function using the signature specified by this format, then store the function reference in
bot_descriptor.img_callback.

#### bot_ctrl_callback_t
Function type for a configuration message callback, as an alias defined by the following function template:<br>
`using bot_ctrl_callback_t = std::function<nlohmann::json (bot_context &context, nlohmann::json &message)>;`

| Parameter  | Type              | Description                                                               |
|------------|-------------------|---------------------------------------------------------------------------|
| `context`  | `bot_context`     | (Read-write) Global context for the bot                                   |
| `message`  | `nlohmann::json &`| (Read) Configuration message, as an nlohmann::json object                 |

returns `nlohmann::json`: A JSON value published to the control channel.

**Note:** The `message` parameter and return value must use the following formats; otherwise, the SDK aborts the bot.

**`message`**
```
  { "to": "<bot_id>", "<key_name>": "<key_value">}
```

**return value**
```
   { "from": "<bot_id>", "<key_name>": "<key_value>" }
```

| Parameter     | Type                | Description                                                      |
|---------------|---------------------|------------------------------------------------------------------|
| `<bot_id>`    | string              | Value specified for the `--id` parameter on the bot command line |
| `<key_name>`  | valid JSON field id | Any valid JSON string                                            |
| `<key_value>` | valid JSON value    | Any valid JSON value (number, string, boolean, object)           |


Whenever it receives a message in the control channel, the SDK invokes a function you define and passes the message to
it in the `message` parameter. Use this function to dynamically configure your bot. For example, you can send a message
that changes parameters that control the image processing function.

To pass settings between the control callback and other functions you define, store the settings in
context.instance_data and pass the context as a parameter. Notice that the image processing callback template is
already set up to do this.

The function template defines a JSON return value to allow your control function to acknowledge receipt of a control
message. The SDK publishes the JSON object back to the control channel. In the return value, you should add a field
that indicates the message is an acknowledgement (**ack**). For example, use the field `"ack": true`.

### Structs

#### `Registry`
From the `prometheus-cpp` library. Data type for the `metrics_registry` member of `bot_context`.

#### `frame_id`
| Field name         | Type     | Description                   |
|--------------------|----------|-------------------------------|
| `i1`               | `int64_t`| Starting id of frame sequence |
| `i2`               | `int64_t`| Ending id of frame sequence   |

Data type for a time-independent identifier for a video frame. The framework assigns values sequentially, which lets 
you refer to a frame or frames without having to use a time code.

When a `frame_id` value is for a single frame, `i1 == i2`. When you want to refer to a sequence of frames in an 
analysis message, you set `i1 < i2`. See the input arguments for [`bot_message()`](#bot_message).

#### `image_frame`
| Field name         | Type                      | Description                         |
|--------------------|---------------------------|-------------------------------------|
 `id`                | `frame_id`                | The id of this frame                |
 `plane_data`        | `uint8_t[MAX_DATA_PLANES]`| Data for a single pixel in the frame|

Data type for a pixel in a video frame:

If an image in the video stream is in a packed pixel format like packed RGB or packed YUV,
then it only needs a single data plane, and all pixels are stored in `plane_data[0]`.

If the image uses planar pixel format like planar YUV or HSV, then every component is stored in its own plane.
For example, if the format is YUV, Y is `plane_data[0]`, U is `plane_data[1]` and V is `plane_data[2]`.

#### `image_metadata`
| Field name         | Type                         | Description                          |
|--------------------|------------------------------|--------------------------------------|
 `width`             | `uint16_t`                   | image width in pixels                |
 `height`            | `uint16_t`                   | image height in pixels               |
 `plane_strides`     | `uint32_t[MAX_IMAGE_PLANES]` | Size of each **stride** in the image.|

Data type for image metadata. The SDK gets this data from the metadata channel if the video input comes
from a channel. If the input is from a file or camera, the metadata comes directly from those sources.
**Note:** A **stride** is an aligned plane.

#### `bot_context`
| Field name         | Type                   | Description                                                 |
|--------------------|------------------------|-------------------------------------------------------------|
| `instance_data`    | pointer                | Pointer to a variable you define. Default value is `nullptr`|
| `frame_metadata`   | `image_metadata`       | Video metadata                                              |
| `mode`             | `execution_mode`       | Execution mode in which the framework should run            |
| `metrics_registry` | `prometheus::Registry` | The `prometheus-cpp` registry for the bot                   |

Use `bot_context` to define a variable you pass to the framework. The framework persists [`bot_context`](#bot_context)
during the lifetime of your bot. In [`bot_context`](#bot_context), use `instance_data` instead of global variables.

Set `instance_data` in your control callback during initialization. The framework
passes it to you when it invokes your callbacks.

#### `bot_descriptor`
| Field name         | Type                  | Description                                          |
|--------------------|-----------------------|------------------------------------------------------|
| `pixel_format`     | `image_pixel_format`  | Pixel format the framework should use for each frame |
| `img_callback`     | `bot_img_callback_t`  | Image processing callback                            |
| `ctrl_callback`    | `bot_ctrl_callback_t` | Control callback                                     |

Information you pass to the SDK by calling [`bot_register()`](#bot_register).

### Enums
#### `execution_mode`

| `enum` constant    |  Description                                               |
|--------------------|------------------------------------------------------------|
| `LIVE`             | Drop frames in order to stay in sync with the video stream |
| `BATCH`            | Pass every frame to the processing callback                |

In live mode, the SDK sends frames to your image callback based on the
incoming frame rate. If your callback lags behind, the framework drops frames to stay in sync with the frame
rate. This mode is available for RTM channel streams, camera input, and files.
**Use live mode for bots running in production.**

In batch (test) mode, the framework waits for your image callback to finish before sending it another
frame, so no frames are dropped. This mode is only available for files.
**Only use `batch` mode for testing.**

#### `image_pixel_format`
| `enum` constant  |  Description                         |
|------------------|--------------------------------------|
| `RGB0`           | Pixels in the input are in RGB format|
| `BGR`            | Pixels in the input are in BGR format|

#### `bot_message_kind`
| `enum` constant   |  Description                         |
|-------------------|--------------------------------------|
| `ANALYSIS`        | Publish message to analysis channel  |
| `DEBUG`           | Publish message to debug channel     |
| `CONTROL`         | Publish message to control channel   |

When you call [`bot_message()`](#bot_message) to publish a message, use `bot_message_kind` to
indicate the destination channel. The framework automatically provides you with these channels
(See [SDK channel names](#sdk-channel-names)).

### Functions

#### Image processing callback

`void name(bot_context &context, const image_frame &frame)`

| Parameter | Type          | Description         |
|-----------|---------------|---------------------|
| `context` | `bot_context` | Global bot context  |
| `frame`   | `image_frame` | Decoded video frame |

returns `void`


See also [`bot_image_callback_t`](#bot_img_callback_t).

#### Configuration message callback
`nlohmann::json <function_name>(bot_context &context, const nlohmann::json *message)`

| Parameter | Type            | Description                                                         |
|-----------|-----------------|---------------------------------------------------------------------|
| `context` | `bot_context`   | Global context you provide                                          |
| `message` | `nlohman::json` | Message received from the control channel, in nlohmann::json format |

returns `nlohmann::json`

The `nlohmann::json` class is part of the **basic json** API in the
[JSON for Modern C++](https://nlohmann.github.io/json/index.html) library.

The framework invokes the callback during initialization, so that you can set up initial conditions.

To send a configuration to the configuration callback, run your bot with the `--config <inline_json>` or
`--config-file <json_file_name>` runtime parameters.

See also [`bot_ctrl_callback_t`](#bot_ctrl_callback_t)

#### bot_message()
`bot_message(bot_context &context, bot_message_kind kind, nlohmann::json &message, const frame_id &id = frame_id{0, 0})`

| Parameter | Type               | Description                                                    |
|-----------|--------------------|----------------------------------------------------------------|
| `context` | `bot_context`      | Global context you provide                                     |
| `kind`    | `bot_message_kind` | Channel in which to publish the message                        |
| `message` | `nlohmann::json`   | Message to publish                                             |
| `id`      | `frame_id`         | Frame identifier or identifiers to associate with this message |

returns `void`

Publishes a message to one of the channels that the framework automatically provides for your bot
(See [SDK channel names](#sdk-channel-names)).

Use the `id` parameter to annotate the message with a frame id or range of ids. If `frame.id1==frame.id2`, the
message is for a single frame. If `frame.id1 < frame.id2`, the message is for a range of frames. Note that
`frame.id1 > frame.id2` is invalid. If you omit the `id` parameter,
the default is `frame_id{0, 0}`, which the framework assumes is the current frame.

#### bot_register()
`bot_register(const bot_descriptor &bot)`

| Parameter | Type             | Description                                                            |
|-----------|------------------|------------------------------------------------------------------------|
| `bot`     | `bot_descriptor` | Specifies the desired pixel format and the image and control callbacks |

Registers your bot with the framework.

#### bot_main()
`bot_main(int argc, char *argv[])`

| Parameter | Type      | Description                   |
|-----------|-----------|-------------------------------|
| `argc`    | `int`     | Number of arguments in `argv` |
| `argv`    | `char*[]` | List of arguments             |

returns `int`

Starts the main event loop in the framework.

## OpenCV-compatible API
The OpenCV compatibility API provides access to OpenCV support in the framework.

The API has one struct and three functions that substitute for functions in the normal API:

| Normal API component   | OpenCV component                | Description                                                            |
|------------------------|---------------------------------|------------------------------------------------------------------------|
| `bot_descriptor`       | `opencv_bot_descriptor`         | Describes your bot to the framework                                    |
| `bot_image_callback_t` | `opencv_bot_image_callback_t`   | Function template for your OpenCV-compatible image processing callback |
| `bot_register`         | `opencv_bot_register`           | Registers your bot with the framework                                  |
| `bot_main`             | `opencv_bot_main`               | Starts your bot's main event loop                                      |

### OpenCV types

#### opencv_bot_img_callback_t
Function type for the image processing callback when used for OpenCV functions. Defined by the following struct:<br>
`using opencv_bot_img_callback_t = std::function<void(bot_context &context, const cv::Mat &img)>;`

| Parameter  | Type          | Description                                                     |
|------------|---------------|-----------------------------------------------------------------|
| `context`  | `bot_context` | (Read-write) Variable containing the global context for the bot |
| `img`      | `cv::Mat`     | (Read-only) OpenCV API `Mat` object                             |
 returns `void`

### OpenCV structs

#### `opencv_bot_descriptor`
| Field name         | Type                 | Description                                                 |
|--------------------|----------------------|-------------------------------------------------------------|
| `img_callback`     | `bot_img_callback_t` | Pointer to your OpenCV-compatible image processing callback |
| `ctrl_callback`    | `bot_ctrl_callback_t`| Pointer to your control callback                            |

You pass a variable of type `opencv_bot_descriptor` to the `opencv_bot_register()` API function that you call when
you start your bot.

Unlike the normal API struct, `opencv_bot_descriptor` doesn't include the `image_pixel_format` field,
because the `cv::Mat` object contains higher-level abstractions for the information.

### OpenCV functions

#### opencv_bot_register()
`opencv_bot_register(const opencv_bot_descriptor &bot)`

| Parameter | Type                    | Description                               |
|-----------|-------------------------|-------------------------------------------|
| `bot`     | `opencv_bot_descriptor` | Specifies the image and control callbacks |

Registers your OpenCV bot with the framework
#### opencv_bot_main()
`opencv_bot_main(int argc, char *argv[])`

| Parameter | Type      | Description                   |
|-----------|-----------|-------------------------------|
| `argc`    | `int`     | Number of arguments in `argv` |
| `argv`    | `char*[]` | List of arguments             |

## Template video bots

The SDK includes template video bots that you can use as a starting point. They are available from the
[Satori Video C++ SDK Examples](https://github.com/satori-com/satori-video-sdk-cpp-examples) GitHub repository.
To get the templates, clone them from the repository. You may reuse them as long as you follow the terms stated in
the `LICENSE.md` file.

The repository contains a subdirectory for each template. You have the following choices:

* `empty-bot`: Basic template for a non-OpenCV bot
* `empty-opencv-bot`: Basic template for an OpenCV bot
* `empty-tensorflow-bot`: Simple template that demonstrates TensorFlow machine learning in a video bot
* `haar-cascades-bot`: Simple template that demonstrates Haar cascades object recognition in a video bot

Each subdirectory has this structure:

```
<template_dir>/
│
├─src
│  └──main.cpp
│
├─CMakeLists.txt
├─Dockerfile
├─README.md
├─conanfile.txt
│
```

| File             | Type        | Description                           |
|------------------|-------------|---------------------------------------|
| `main.cpp`       | C++ program | Template program for your code        |
| `CMakeLists.txt` | CMake       | CMake build file                      |
| `Dockerfile`     | Docker      | Docker configuration file             |
| `conanfile.txt`  | conan       | conan package recipe for the template |

**Note:** You don't have to use Docker or CMake, but you do have to use `conan` to install the SDK libraries and their
dependencies.

## Supported video data formats
The framework supports the following frameworks:<br>
• MPEG-4 Part 10 (H.264) and MPEG-4 Part 2 compressed video. File extension is `.mp4`.<br>
• Matroska Multimedia Container (**MKV**). File extension is `.mkv`.<br>
• WebM. File extension is `.webm`.<br>

## SDK channel names
The SDK automatically provides channels for publishing and receiving information. Their names are
based on the incoming video stream channel you provide. The following table lists the channel names that the SDK
uses for an incoming video stream channel named `stream_channel`:

| Use                                           | `enum` constant | I/O    | Channel name              |
|-----------------------------------------------|-----------------|--------|---------------------------|
| Incoming video stream                         |      -          | Input  | `stream_channel`          |
| Image processing results (analysis)           | `ANALYSIS`      | Output | `stream_channel/analysis` |
| Codec and stream information                  |      -          | Output | `stream_channel/metadata` |
| Debug messages                                | `DEBUG`         | Output | `stream_channel/debug`    |
| Control messages for configuring bot behavior | `CONTROL`       | Input  | `stream_channel/control`  |

## Video bot command-line parameters
Use command-line parameters to configure the operation of your video bot. You can also use the `--config-file` or the
`--config` parameter to pass configuration settings to your control callback.

### Syntax

```
<bot_name> [options] --input-video-file <vfile> --endpoint <wsendpoint> --appkey <key> --port <port> --input-channel <input_channel_name>
<bot_name> [options] --input-replay-file <rfile> --endpoint <wsendpoint> --appkey <key> --port <port> --input-channel <input_channel_name>
<bot_name> [options] --input-camera --endpoint <wsendpoint> --appkey <key> --port <port> --input-channel <input_channel_name>
<bot_name> [options] --input-url --endpoint <wsendpoint> --appkey <key> --port <port> --input-channel <input_channel_name>
```

### Options
```
       [--loop]
       [--batch]
       [--time-limit <tlimit>]
       [--frames-limit <flimit>]
       [--input-resolution [<res> | original]
       [--keep-proportions [true | false]]
       [--id <bot_id>]
       [--config-file <json_configfile> | --config <json_config>]
       [--analysis-file <analysisfile>]
       [--debug-file <debugfile>]
       [-v <verbosity>]
       [--metrics-bind-address <metrics_url>
       [--metrics-push-job     <metrics_job_value>]
       [--metrics-push-instance <metrics_instance_value>]
       [--pool <bot_service_pool>
```

### Parameters
`--endpoint <wsendpoint>`

Satori channel endpoint for the output channel. This value is
available from your project page in the Satori Dev Portal. The format is
`<val>.api.satori.com`, where `<val>` is unique to your project.

`--appkey <key>`

Appkey for your project. This value is available from your
project page in the Satori Dev Portal. The format is a 32-character
hexadecimal value.

`--port <port>`

Port to use for the channel. The default is 443.

`--input-channel <input_channel_name>`

Video input channel name. The SDK uses this
name as the base name for the analysis, control, debug, and metrics
channels it provides you.

`--input-video-file <vfile>`

Name of a file that contains compressed video input. The
bot uses this file instead of an input video channel. The file must
be in `.mp4`, `.mkv`, or `.webm` format.

`--input-replay-file <rfile>`

Name of a file that contains compressed video input. The bot
uses this file instead of an input video channel. The file must be in the
form of Satori CBOR messages.

`--input-camera`

Indicates that the compressed video input comes from the macOS X camera that's part of the computer running the bot. The
video from the camera is used instead of the input video channel.

`--loop`

Indicates that the input file contains a loop of video. This option is
only available when you use the `--input-video-file` option.

`--batch`

Batch execution mode. This option is only available when you use
a file as input. Use only for testing. If not specified, the bot runs in live mode.

`--time-limit <tlimit>`

Time limit for bot execution in seconds. After it reaches the limit, the bot stops.

`--frames-limit <flimit>`

Number of frames for the bot to process. After it reaches the limit, the bot stops.

`--input-resolution [<res> | original]`

Input resolution to use in processing of the input stream,
in pixels. The format is `<width>x<height>`, and the default is 320x480. If
you specify original, the video bot uses the format recorded in the input
stream.

`--keep-proportions [true | false]`

Keep (`true`) or ignore (`false`) the proportions of the input stream. The default is `true`.

`--id <bot_id>`

A JSON-compatible text string that identifies the bot. Messages that you publish from the bot contain the
attribute ` "bot" : "<bot_id>"`.

`--config-file <configfile>`

File that contains your configuration options in JSON format. The SDK passes the JSON in this file to your
control callback.

`--config <json_config>`
Options in JSON format. The bot passes the JSON to your control callback.

`--analysis-file <analysisfile>`

Write analytics messages to a file instead of the default analytics channel.

`--debug-file <debugfile>`

Write debug messages to a file instead of the default debug channel.

`-v <verbosity>`

Amount of information to put into the log file

| Value                                               |  Meaning                                 |
|-----------------------------------------------------|------------------------------------------|
|  <center>OFF</center>                               | No messages                              |
|  <center>0 or INFO</center>                         | Minimal information message              |
|  <center>1 or DEBUG</center>                        | More messages (useful for debugging)     |
|  <center>2 through 5</center>                       | Increasing amounts of information        |
|  <center>WARNING</center>                           | Warning, error, and fatal messages only  |
|  <center>ERROR</center>                             | Error and fatal messages only            |
|  <center>FATAL</center>                             | Fatal messages only                      |

When you build a bot for production by running `cmake -DCMAKE_BUILD_TYPE=Release ../`, the verbosity
defaults to `0`, and you don't have to set it explicitly. Similarly, the default for
`cmake -DCMAKE_BUILD_TYPE=Debug ../` is `1`.

`--metrics-bind-address <metrics_url>`

HTTP address of the Prometheus metrics server. Format is `http://<address>:<port>`. To view the metrics,
open `http://<address>:<port>/metrics` in a browser.

`--metrics-push-job <metrics_job_value>`
Add this string as the value of the job property in metrics data that the bot pushes to the Prometheus server.

`--metrics-push-instance <metrics_instance_value>`
Add this string as the value of the instance property in metrics data that the bot pushes to the Prometheus server.

`--pool <bot_service_pool>`

Start the bot as a service in the specified pool. The bot advertises its capacity on RTM channel and listens for
pool manager assignments.

### Command line notes

#### Input files

The `--input-video-file`, `input-replay-file`, and `input-camera` options are
mutually exclusive.

#### Bot processing limits

You can specify both `--time-limit` and `--frame-limit` at the same time.

#### Bot configuration

`--config-file` and `--config` are mutually exclusive.

Add configuration options as properties. For example, this configuration specifies Haar classifier files as
child properties of the "model_files" property:

```
{
  "model_files": {
    "../../models/haarcascade_frontalface_default.xml": "a face",
    "../../models/haarcascade_smile.xml": "a smile",
  }
}

```
In your configuration, you can use any property key except "action" and "body". Avoid using the property value
"configure".

## Command-line tools
The Video SDK command-line tools publish, record, and playback video streams.

The most important tool is `satori_video_publisher`, which publishes streaming video from a camera or file to a
Satori channel. It's the primary tool for creating the input to a video bot.

The `satori_video_recorder` tool records streaming video to a file. The source can be another video file or a camera.

To play back a video file or display camera input, use the `satori_video_player` tool.
### `satori_video_publisher`

Publish a video stream to a channel

#### Syntax
```
satori_video_publisher [options] --input-video-file <vfile> --endpoint <wsendpoint> --appkey <key> --output-channel <output_channel_name> --port <wsport>
satori_video_publisher [options] --input-replay-file <rfile> --endpoint <wsendpoint> --appkey <key> --output-channel <output_channel_name> --port <wsport>
satori_video_publisher [options] --input-camera --endpoint <wsendpoint> --appkey <key> --output-channel <output_channel_name> --port <wsport>
satori_video_publisher [options] --input-url <URL> --endpoint <wsendpoint> --appkey <key> --output-channel <output_channel_name> --port <wsport>
```

#### Options
```
        [--loop]
        [--output-resolution [<res>|original]]
        [--keep-proportions [true | false]]
        [--metrics-push-job     <metrics_job_value>]
        [--metrics-push-instance <metrics_instance_value>]
        [-v <verbosity>]
        [--help]
```

#### Parameters
`--input-video-file <vfile>`

Publish `<vfile>`, the path-relative name of a file that contains compressed video. Valid formats are `mp4`,`mkv`,
and `webm`.

`--input-replay-file <rfile>`

Publish `<rfile>` to the channel, the path-relative name of a file
that contains RTM messages representing streaming video.

`--input-camera`

Publish streaming video from the macOS laptop camera.

`--input-url <URL>`

Publish video from a URL that represents a video source. Use this parameter to publish video from a webcam.

`--endpoint <wsendpoint>`

Destination channel endpoint. Get this value from your project details in the Satori Dev Portal. The
format is `<hash>.api.satori.com`, where `<hash>` is a value that's unique to your project.

`--appkey <key>`

Destination channel appkey. Get this value from your project details in the Satori Dev Portal. `<key>` is a
32-digit hexadecimal value that's unique to your project.

`--port <wsport>`

Port to use for the channel. The default is 443.

`--output-channel <output_channel_name>`

Destination channel name. `<output_channel_name>` is the root name for the other channels that the SDK uses. See
[SDK channel names](#sdk-channel-names).

`--loop`

For `--input-video-file` or `--input-replay-file`, tells the tool to publish the file in a continuous loop.

`--output-resolution res`

Publish video with the specified output resolution. If set to `original`, publish with the input resolution. The
format of `<res>` is `<width>x<height>` in pixels. The default is `320x480`.

`--keep-proportions [true | false]`

Keep (`true`) or ignore (`false`) the original proportions of the source.

`-v <verbosity>`

Amount of information to put into the log file

| Value                              |  Meaning                                 |
|------------------------------------|------------------------------------------|
|  <center>OFF</center>              | No messages                              |
|  <center>0 or INFO</center>        | Minimal information message              |
|  <center>1 or DEBUG</center>       | More messages (useful for debugging)     |
|  <center>2 through 5</center>      | Increasing amounts of information        |
|  <center>WARNING</center>          | Warning, error, and fatal messages only  |
|  <center>ERROR</center>            | Error and fatal messages only            |
|  <center>FATAL</center>            | Fatal messages only                      |

When you build a bot for production by running `cmake -DCMAKE_BUILD_TYPE=Release ../`, the verbosity
defaults to `0`, and you don't have to set it explicitly. Similarly, the default for
`cmake -DCMAKE_BUILD_TYPE=Debug ../` is `1`.

`--help`

Display usage hints for the tool.

`--metrics-push-job <metrics_job_value>`

Add this string as the value of the job property in metrics data that the tool writes to the push server.

`--metrics-push-instance <metrics_instance_value>`

Add this string as the value of the instance property in metrics data that the tool writes to the push server.

### `satori_video_player`
Play video from a source in a GUI window.

#### Syntax
```
satori_video_player [options] --endpoint <wsendpoint> --appkey <key> --port <wsport> --input-channel <input_channel_name>
satori_video_player [options] --input-video-file <vfile>
satori_video_player [options] --input-replay-file <rfile>
satori_video_player [options] --input-camera
satori_video_player [options] --input-url <URL>
```

#### Options
```
        [--loop]
        [--time-limit <tlimit>]
        [--frames-limit <flimit]
        [--input-resolution [<res> | original]]
        [--output-resolution [<res>|original]]
        [--keep-proportions [true | false]]
        [--help]
        [-v <verbosity>]
```

#### Parameters

`--endpoint <wsendpoint>`

Endpoint for the input channel. Get this value from your project details in the Satori Dev Portal. The
format is `<hash>.api.satori.com`, where `<hash>` is a value that's unique to your project.

`--appkey <key>`

App key for the input channel. Get this value from your project details in the Satori Dev Portal. `<key>` is a
32-digit hexadecimal value that's unique to your project.

`--port <wsport>`

Port for the WebSocket connection to the input channel. The default is 443.

`--channel <input_channel_name>`

Name of the channel containing the video you want to play.

`--input-video-file <vfile>`

Play `<vfile>`, the path-relative name of a file that contains compressed video. Valid formats are `mp4`,`mkv`, and `webm`.

`--input-replay-file <rfile>`

Play `<rfile>`, the path-relative name of a file that contains RTM messages representing streaming video.

`--input-camera`

Play video from the macOS laptop camera.

`--input-url <URL>`

Play video from a URL that represents a video source. Use this parameter to play video from a webcam.

`--loop`

For `--input-video-file` or `--input-replay-file`, tells the tool to play the video in a loop.

**Note:** If you specify this parameter and the file *doesn't* contain a video loop, the tool hangs.

`--time-limit <tlimit>`

After `<tlimit>` seconds, the tool exits.

`--frame-limit <flimit>`

The tool exits after processing `<flimit>` frames.

`--input-resolution [<res>|original]`

Resolution, in pixels, of the input source. If set to `original`, use the input resolution from the file or channel.
The format of `<res>` is `<width>x<height>` in pixels. The default is `320x480`.

`--output-resolution res`

Play the video with the specified output resolution. If set to `original`, publish with the input resolution. The
format of `<res>` is `<width>x<height>` in pixels. The default is `320x480`.

`--keep-proportions [true | false]`

Keep (`true`) or ignore (`false`) the original proportions of the source.

`-v <verbosity>`

Amount of information to put into the log file

| Value                              |  Meaning                                 |
|------------------------------------|------------------------------------------|
|  <center>OFF</center>              | No messages                              |
|  <center>0 or INFO</center>        | Minimal information message              |
|  <center>1 or DEBUG</center>       | More messages (useful for debugging)     |
|  <center>2 through 5</center>      | Increasing amounts of information        |
|  <center>WARNING</center>          | Warning, error, and fatal messages only  |
|  <center>ERROR</center>            | Error and fatal messages only            |
|  <center>FATAL</center>            | Fatal messages only                      |

When you build a bot for production by running `cmake -DCMAKE_BUILD_TYPE=Release ../`, the verbosity
defaults to `0`, and you don't have to set it explicitly. Similarly, the default for
`cmake -DCMAKE_BUILD_TYPE=Debug ../` is `1`.

`--help`

Display usage hints for the tool.

### `satori_video_recorder`
Record video input to a file.

#### Syntax

```
satori_video_recorder [options] --endpoint <wsendpoint> --appkey <key> --input-channel <input_channel_name> --port <wsport> --output-video-file <ofile>
satori_video_recorder [options] --input-camera --output-video-file <ofile>
satori_video_recorder [options] --input-url <URL> --output-video-file <ofile>
```
#### Options

```
        [--loop]
        [--time-limit <tlimit>]
        [--frames-limit <flimit]
        [--input-resolution [<res> | original]]
        [--keep-proportions [true | false]]
        [--reserved-index-space <space>]
        [-v <verbosity>]
        [--help]
```

#### Parameters

`--endpoint <wsendpoint>`

Endpoint for the input channel. Get this value from your project details in the Satori Dev Portal. The
format is `<hash>.api.satori.com`, where `<hash>` is a value that's unique to your project.

`--appkey <key>`

App key for the input channel. Get this value from your project details in the Satori Dev Portal. `<key>` is a
32-digit hexadecimal value that's unique to your project.

`--port <wsport>`

Port for the WebSocket connection to the input channel. The default is 443.

`--channel <input_channel_name>`

Name of the channel containing the video you want to record.

`--input-video-file <vfile>`

Record `<vfile>`, the path-relative name of a file that contains compressed video. Valid formats are `mp4`,`mkv`, and `webm`.

`--input-replay-file <rfile>`

Record `<rfile>`, the path-relative name of a file that contains RTM messages representing streaming video.

`--input-camera`

Record video from the macOS laptop camera.

`--time-limit <tlimit>`

After `<tlimit>` seconds, the tool exits.

`--frame-limit <flimit>`

The tool exits after processing `<flimit>` frames.

`--input-resolution [<res>|original]`

Resolution, in pixels, of the input source. If set to `original`, record with the input resolution specified in
the input stream. The format of `<res>` is `<width>x<height>` in pixels. The default is `320x480`.

`--output-resolution res`

Record the video with the specified output resolution. If set to `original`, publish with the input resolution. The
format of `<res>` is `<width>x<height>` in pixels. The default is `320x480`.

`--keep-proportions [true | false]`

Keep (`true`) or ignore (`false`) the original proportions of the source.

--`output-video-file <ofile>`

Record to `<ofile>`. The name is path-relative.

`--reserved-index-space <space>`

Space, in bytes, to reserve at the beginning of the output file for **cues** (indexes) that improve seeking. In most
cases, 50000 is enough for one hour of video. If the input format is Matroska (.mkv) and you don't specify a value
for `<space>`, the tool writes cues to the end of the file.

`-v <verbosity>`

Amount of information to put into the log file

| Value                              |  Meaning                                 |
|------------------------------------|------------------------------------------|
|  <center>OFF</center>              | No messages                              |
|  <center>0 or INFO</center>        | Minimal information message              |
|  <center>1 or DEBUG</center>       | More messages (useful for debugging)     |
|  <center>2 through 5</center>      | Increasing amounts of information        |
|  <center>WARNING</center>          | Warning, error, and fatal messages only  |
|  <center>ERROR</center>            | Error and fatal messages only            |
|  <center>FATAL</center>            | Fatal messages only                      |

When you build a bot for production by running `cmake -DCMAKE_BUILD_TYPE=Release ../`, the verbosity
defaults to `0`, and you don't have to set it explicitly. Similarly, the default for
`cmake -DCMAKE_BUILD_TYPE=Debug ../` is `1`.

`--help`

Display usage hints for the \*.
