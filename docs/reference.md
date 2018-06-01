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
* [Example video bots](#example-video-bots)
* [Supported video data formats](#supported-video-data-formats)
* [SDK channel names](#sdk-channel-names)
* [Video bot command-line syntax](#video-bot-command-line-syntax)
* [Command-line tools](#command-line-tools)
    * [satori_video_publisher](#satori_video_publisher)
    * [satori_video_player](#satori_video_player)
    * [satori_video_recorder](#satori_video_recorder)

## SDK API

### Base includes
The base API for the video SDK is declared in the header file [`video_bot.h`](../include/satorivideo/video_bot.h).

The SDK also uses the following libraries:

| Library                                                                             | Header file  |
|-------------------------------------------------------------------------------------|--------------|
| [JSON for Modern C++](https://nlohmann.github.io/json/index.html)                   | `json.hpp`   |
| [Loguru Header-only C++ Logging Library](https://emilk.github.io/loguru/index.html) | `loguru.hpp` |
| [Prometheus Library for Modern C++](https://github.com/jupp0r/prometheus-cpp)       | (several)    |

The bot build process uses `conan` to install these files.

### OpenCV includes
If you want to use the OpenCV image processing library in your video bot, you also need the following libraries:

| File                                                             | Description                |
|------------------------------------------------------------------|----------------------------|
| [`opencv_bot.h`](../include/satorivideo/opencv/opencv_bot.h)     | OpenCV video bot API       |
| [`opencv_utils.h`](../include/satorivideo/opencv/opencv_utils.h) | OpenCV video bot utilities |
| `opencv.hpp` for OpenCV (installed during the build)             | OpenCV API                 |

The bot build process uses `conan` to install these files.

**Note:** The OpenCV library is in the `opencv2` directory for historical reasons. The video SDK actually uses
the latest version 3 library.

### Constants

#### `MAX_IMAGE_PLANES`
| Variable/type name | Type     | Value |Description                |
|--------------------| :------: | :---: | :------------------------ |
| `MAX_IMAGE_PLANES` | `uint8_t`| 4     | Max number of data planes |

Unsigned integer set to 4. Defines the maximum number of data planes a frame can contain.

### Function types

#### `bot_img_callback_t`
Function type for an image processing callback, in the form of an alias defined by the following:<br>
`using bot_img_callback_t = std::function<void(bot_context &context, const image_frame &frame)>;`

| Parameter     | Type          | Description                             |
|---------------|---------------|-----------------------------------------|
| `context`     | `bot_context` | (Read-write) Global context for the bot |
| `image_frame` | `image_frame` | (Read-only) Video frame                 |

returns `void`

After it decodes a video frame from the incoming video stream, the SDK invokes the image processing callback and passes
it the frame. Use this function to process the video frame.

Create a function using the signature specified by this format, then store the function reference in
`bot_descriptor.img_callback`.

#### `bot_ctrl_callback_t`
Function type for a command processing callback, in the form of an alias defined by the following:<br>
`using bot_ctrl_callback_t = std::function<nlohmann::json (bot_context &context, nlohmann::json &message)>;`

| Parameter  | Type             | Description                                         |
|------------|------------------|-----------------------------------------------------|
| `context`  | `bot_context`    | (Read-write) Global context for the bot             |
| `message`  | `nlohmann::json` | (Read) Control message, as an nlohmann::json object |

returns `nlohmann::json`: A JSON value published to the control channel.

Whenever it receives a message in the control channel, the SDK invokes a function you define and passes the message to
it in the `message` parameter. Use this function to dynamically configure your bot. For example, you can send a message
that changes parameters that control the image processing function.

To pass settings between the command processing callback and other functions you define, store the settings in
context.instance_data and pass the context as a parameter. Notice that the image processing callback template is
already set up to do this.

The function template defines a JSON return value to allow your command processing function to acknowledge receipt of a 
control message. The SDK publishes the JSON object back to the control channel. In the return value, you should add a 
field that indicates the message is an acknowledgement (**ack**). For example, use the field `"ack": true`.

**Command processing fields**

In these field descriptions, `<bot_id>` is the value of the `--id` parameter on the bot command line.

**Incoming Control message**

`message` includes the following fields:

* **`"to": "<bot_id>"`**: **Required** Identifies the destination bot. This allows multiple video bots, each with its
own identifier, to use the same control channel. If this field isn't specified, the SDK returns an error and doesn't
pass the message to the command processing callback.

* **`"<key>": {<value>}`**: **Optional** Specifies the configuration being passed. Using a single JSON object to
specify all of your configuration data simplifies the process of checking for a valid message and moving the data to
the bot context in your bot. See the [Satori Video SDK for C++: Tutorial](tutorial.md) for an example of this.

For example:
```json
{ "to": "my_bot", "parameters": { "featureSize": 5.0}}
```

**Return value message**

The return value includes the following fields:

```
"from": "<bot_id>", "to": "<bot_id>", "<ack_key>": <ack_value>
```

* **`"to": "<bot_id>"`**: **Required** Because the SDK can't know if a control channel message is an incoming
configuration or an outgoing acknowledgement, it requires the `"to"` field in **all** control channel messages.
* **`"from": "<bot_id>"`**: **Required** The SDK verifies that messages it publishes to the control channel have a
`"from"` field that let subscribers know the source of the message.
* **`"<ack_key>"`**: **Optional** Key for the acknowledgement field
* **`<ack_value>`**: **Optional** Value for the acknowledgement field

The ack field in a control message indicates that the message is a response to a request for a configuration update.

For example:
```json
{ "from": "my_bot", "to": "my_bot", "ack": true }
```

### Structs

#### `Registry`
See the `prometheus-cpp` library. Struct for the `metrics_registry` member of `bot_context`.

#### `frame_id`
| Member | Type      | Description                   |
|--------|-----------|-------------------------------|
| `i1`   | `int64_t` | Starting id of frame sequence |
| `i2`   | `int64_t` | Ending id of frame sequence   |

Struct for the time-independent identifier of a video frame. The SDK assigns values sequentially, which lets
you refer to a frame or frames in sequence without having to use a time code.

When a `frame_id` value is for a single frame, i1 and i2 are equal. When you want to refer to a sequence of frames in an
analysis message, set i1 to the beginning frame id, and i2 to the ending frame id. See the input arguments for
[`bot_message()`](#bot_message).

#### `image_frame`
| Member       | Type                        | Description                           |
|--------------|-----------------------------|---------------------------------------|
| `id`         | `frame_id`                  | The id of this frame                  |
| `plane_data` | `uint8_t[MAX_IMAGE_PLANES]` | Pixel values for the frame            |

Struct for a single image frame.

The organization of `plane_data` depends on the pixel format in use:

If the images in the video stream use a packed pixel format like packed RGB or packed YUV,
`plane_data` has a length of 1.

If the images use a planar pixel format like planar YUV or HSV, then each frame uses one plane for each channel.
For example, if the format is YUV, Y is `plane_data[0]`, U is `plane_data[1]` V is `plane_data[2]`, and `plane_data`
has a length of 3.

#### `image_metadata`
| Member          | Type                         | Description                                |
|-----------------|------------------------------|--------------------------------------------|
| `width`         | `uint16_t`                   | image width in pixels                      |
| `height`        | `uint16_t`                   | image height in pixels                     |
| `plane_strides` | `uint32_t[MAX_IMAGE_PLANES]` | Size of the **image stride** for the plane |

Struct for image metadata. The SDK gets this data from the metadata channel if the video input comes
from an RTM channel. If the input is from a file or camera, the metadata comes directly from those sources.

Each element in `image_frame.plane_data` has a corresponding element in `image_metadata.plane_strides`. The
`plane_strides` element contains the size of the `plane_data` image stride.

**Note:** An **image stride** is the total length of the array element containing the image data, including any padding
that the source added. Sources may align elements on word boundaries or cache line sizes to improve performance.

#### `bot_context`
| Member             | Type                   | Description                                                |
|--------------------|------------------------|------------------------------------------------------------|
| `instance_data`    | pointer                | Pointer to a member you define. Default value is `nullptr` |
| `frame_metadata`   | `image_metadata`       | Video metadata                                             |
| `mode`             | `execution_mode`       | Execution mode in which the SDK should run                 |
| `metrics_registry` | `prometheus::Registry` | The `prometheus-cpp` registry for the bot                  |

Use `bot_context.instance_data` instead of global variables to store globals that you want to persist during the
lifetime of your bot.

For example, use `bot_context.instance_data` to store configuration data you receive from the control channel. In
your command processing callback, move the data from the incoming message to `instance_data`. Because the SDK passes
`bot_context` to your callbacks by reference, you can use the updated data in your image processing callback.

#### `bot_descriptor`
| Member          | Type                  | Description                                    |
|-----------------|-----------------------|------------------------------------------------|
| `pixel_format`  | `image_pixel_format`  | Pixel format the SDK should use for each frame |
| `img_callback`  | `bot_img_callback_t`  | Image processing callback                      |
| `ctrl_callback` | `bot_ctrl_callback_t` | Control callback                               |

Information you pass to the SDK by calling [`bot_register()`](#bot_register).

### Enums
#### `execution_mode`

| `enum` constant |  Description                                               |
|-----------------|------------------------------------------------------------|
| `LIVE`          | Drop frames in order to stay in sync with the video stream |
| `BATCH`         | Pass every frame to the processing callback                |

In live mode, the SDK sends frames to your image callback based on the
incoming frame rate. If your callback lags behind, the SDK drops frames to stay in sync with the frame
rate. This mode is available for RTM channel streams, camera input, and files.
**Use live mode for bots running in production.**

In batch (test) mode, the SDK waits for your image callback to finish before sending it another
frame, so no frames are dropped. This mode is only available for files.
**Only use `batch` mode for testing.**

For example:
```cpp
bot_context.mode = execution_mode.LIVE;
```

#### `image_pixel_format`
| `enum` constant |  Description                          |
|-----------------|---------------------------------------|
| `RGB0`          | Pixels in the input are in RGB format.|
| `BGR`           | Pixels in the input are in BGR format |

The SDK handles two types of video streaming formats. If you use the non-OpenCV version of the SDK APIs, you have to
specify the format yourself, using the `image_pixel_format` enum:
* `RGB0`: Each pixel is represented by 4 bytes, one each for the red, green, and blue value, and one byte containing
  zeros.
* `BGR`: Each pixel is represented by 3 bytes, one each for blue, green, and red.

#### `bot_message_kind`
| `enum` constant |  Description                        |
|-----------------|-------------------------------------|
| `ANALYSIS`      | Publish message to analysis channel |
| `DEBUG`         | Publish message to debug channel    |
| `CONTROL`       | Publish message to control channel  |

When you call [`bot_message()`](#bot_message) to publish a message, use `bot_message_kind` to
indicate the destination channel. The SDK automatically provides you with these channels
(See [SDK channel names](#sdk-channel-names)).

For example:
```cpp
sv::bot_message(context, sv::bot_message_kind::ANALYSIS, analysis_message));
```

If you use [Output options](#output-options) when you invoke your bot, `bot_message_kind` writes the message to the
corresponding file instead of a channel. For example, `bot_message_kind.ANALYSIS` writes the message to the file
specified by `--analysis-file <filename>`.

### Functions

#### Image processing callback

`void name(bot_context &context, const image_frame &frame)`

| Parameter | Type          | Description         |
|-----------|---------------|---------------------|
| `context` | `bot_context` | Global bot context  |
| `frame`   | `image_frame` | Decoded video frame |

returns `void`

A callback you create to process video frames produced by the SDK. The section
[`bot_image_callback_t`](#bot_img_callback_t) describes the use of this function.

#### Configuration message callback
`nlohmann::json some_name(bot_context &context, const nlohmann::json *message)`

| Parameter | Type            | Description                                                         |
|-----------|-----------------|---------------------------------------------------------------------|
| `context` | `bot_context`   | Global context you provide                                          |
| `message` | `nlohman::json` | Message received from the control channel, in nlohmann::json format |

returns `nlohmann::json`

A callback you create to process messages the SDK receives from the control channel. The section
[`bot_ctrl_callback_t`](#bot_ctrl_callback_t) describes the use of this function.

#### bot_message()
`bot_message(bot_context &context, bot_message_kind kind, nlohmann::json &message, const frame_id &id = frame_id{0, 0})`

| Parameter | Type               | Description                                                    |
|-----------|--------------------|----------------------------------------------------------------|
| `context` | `bot_context`      | Global context you provide                                     |
| `kind`    | `bot_message_kind` | Channel in which to publish the message                        |
| `message` | `nlohmann::json`   | Message to publish                                             |
| `id`      | `frame_id`         | Frame identifier or identifiers to associate with this message |

returns `void`

Publishes a message to one of the channels the SDK automatically provides for your bot
(See [SDK channel names](#sdk-channel-names)).

If you use [Output options](#output-options) when you invoke your bot, `bot_message()` writes the message to a file
instead of a channel.

Use the `id` parameter to annotate the message with a frame id or range of ids. If `frame.id1==frame.id2`, the
message is for a single frame. If `frame.id1 < frame.id2`, the message is for a range of frames. Note that
`frame.id1 > frame.id2` is invalid. If you omit the `id` parameter,
the default is `frame_id{0, 0}`, which the SDK assumes is the current frame.

For example:
```cpp
bot_message(context, bot_message_kind::DEBUG, message, );
```

This API call sends a message to the DEBUG channel with `frame_id` set to `[0,0]`*[]:

#### bot_register()
`bot_register(const bot_descriptor &bot)`

| Parameter | Type             | Description                                                            |
|-----------|------------------|------------------------------------------------------------------------|
| `bot`     | `bot_descriptor` | Specifies the desired pixel format and the image and control callbacks |

Registers your bot with the SDK.

For example:
```cpp
    namespace sv = satori::video;
    /* some code */
    void image_process_function(sv::bot_context &context, const sv::image_frame &frame){}
    /* more code */
    nlohmann::json control_message_function(sv::bot_context &context, const nlohmann::json &command_message) {
       return nullptr;
    }
    /* more code */
    int main(int argc, char *argv[]) {
        sv::bot_descriptor descr{sv::image_pixel_format::RGB0, &image_process_function, &control_message_function);
        sv::bot_register(descr);
        return sv::bot_main(argc, argv);
    }
```

#### bot_main()
`bot_main(int argc, char *argv[])`

| Parameter | Type      | Description                   |
|-----------|-----------|-------------------------------|
| `argc`    | `int`     | Number of arguments in `argv` |
| `argv`    | `char*[]` | List of arguments             |

returns `int`

Starts the main event loop in the SDK. See [bot_register](#bot_register) for an example.

## OpenCV-compatible API
The OpenCV compatibility API provides access to OpenCV support in the SDK.

The API has one struct and three functions that substitute for functions in the non-OpenCV API:

| non-OpenCV API component   | OpenCV component                | Description                                                            |
|----------------------------|---------------------------------|------------------------------------------------------------------------|
| `bot_descriptor`           | `opencv_bot_descriptor`         | Describes your bot to the SDK                                          |
| `bot_image_callback_t`     | `opencv_bot_image_callback_t`   | Function template for your OpenCV-compatible image processing callback |
| `bot_register`             | `opencv_bot_register`           | Registers your bot with the SDK                                        |
| `bot_main`                 | `opencv_bot_main`               | Starts your bot's main event loop                                      |

### OpenCV types

#### opencv_bot_img_callback_t
Function type for an OpenCV-compatible image processing callback, in the form of an alias defined by
the following:<br>
`using bot_img_callback_t = std::function<void(bot_context &context, const cv::Mat &img)>;`

| Parameter  | Type          | Description                                                     |
|------------|---------------|-----------------------------------------------------------------|
| `context`  | `bot_context` | (Read-write) Variable containing the global context for the bot |
| `img`      | `cv::Mat`     | (Read-only) OpenCV API `Mat` object                             |

returns `void`

After it decodes a video frame from the incoming video stream, the SDK invokes the image processing callback and
passes it the OpenCV `Mat` object. Use this function to process the object.

Create a function using the signature specified by this format, then store the function reference in
`opencv_bot_descriptor.img_callback`.

### OpenCV structs

#### `opencv_bot_descriptor`
| Member             | Type                        | Description                                                 |
|--------------------|-----------------------------|-------------------------------------------------------------|
| `img_callback`     | `opencv_bot_img_callback_t` | Pointer to your OpenCV-compatible image processing callback |
| `ctrl_callback`    | `bot_ctrl_callback_t`       | Pointer to your control callback                            |

You pass a variable of type `opencv_bot_descriptor` to the `opencv_bot_register()` API function that you call when
you start your bot.

Unlike the non-OpenCV API struct, `opencv_bot_descriptor` doesn't include the `image_pixel_format` field,
because the `cv::Mat` object contains that information.

Notice that the OpenCV-compatible API uses the same definition for the command processing function as the non-OpenCV API.
The `opencv_bot_descriptor.ctrl_callback` member has the same type as the regular API struct.

### OpenCV functions

#### opencv_bot_register()
`opencv_bot_register(const opencv_bot_descriptor &bot)`

| Parameter | Type                    | Description                               |
|-----------|-------------------------|-------------------------------------------|
| `bot`     | `opencv_bot_descriptor` | Specifies the image and control callbacks |

Registers your OpenCV bot with the SDK.

For example:
```cpp
    namespace sv = satori::video;
    /* some code */
    void image_process_function(sv::bot_context &context, const cv::Mat &image){}
    /* more code */
    nlohmann::json control_message_function(sv::bot_context &context, const nlohmann::json &command_message) {
       return nullptr;
    }
    /* more code */
    int main(int argc, char *argv[]) {
        sv::opencv_bot_descriptor descr{&image_process_function, &control_message_function);
        sv::opencv_bot_register(descr);
        return sv::opencv_bot_main(argc, argv);
    }
```


#### opencv_bot_main()
`opencv_bot_main(int argc, char *argv[])`

| Parameter | Type      | Description                   |
|-----------|-----------|-------------------------------|
| `argc`    | `int`     | Number of arguments in `argv` |
| `argv`    | `char*[]` | List of arguments             |

returns `int`

Starts the main event loop in the SDK. See [opencv_bot_register](#opencv_bot_register) for an example.

## Example video bots

The SDK includes example video bots that you can use as a starting point. They are available from the
[Satori Video C++ SDK Examples](https://github.com/satori-com/satori-video-sdk-cpp-examples) GitHub repository.
To get the templates, clone them from the repository. You may reuse them as long as you follow the terms stated in
the `LICENSE` file.

The repository contains a subdirectory for each template. You have the following choices:

* `empty-bot`: A non-OpenCV bot
* `empty-opencv-bot`: An OpenCV bot
* `empty-tensorflow-bot`: Simple example that demonstrates TensorFlow machine learning in a video bot
* `haar-cascades-bot`: Simple example that demonstrates Haar cascades object recognition in a video bot
* `motion-detector-bot`: Simple example of a bot that detects features in video frames and publishes their contours to
the analysis channel. Other bots can subscribe to the channel, receive the analysis messages, and track a contour over
time to determine if they represent moving objects. This bot is also the basis of the
[Satori Video SDK for C++: Tutorial](tutorial.md).

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
| `main.cpp`       | C++ program | C++ code                              |
| `CMakeLists.txt` | CMake       | CMake settings                        |
| `Dockerfile`     | Docker      | Docker configuration file             |
| `conanfile.txt`  | conan       | conan package recipe for the example  |

**Note:** You don't have to use Docker or CMake, but you do have to use `conan` to install the SDK libraries and their
dependencies.

## Supported video data formats
The SDK supports the following formats:<br>
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
| Codec and stream information                  |      -          | Input  | `stream_channel/metadata` |
| Image processing results (analysis)           | `ANALYSIS`      | Output | `stream_channel/analysis` |
| Control messages for configuring bot behavior | `CONTROL`       | I/O    | `stream_channel/control`  |
| Debug messages                                | `DEBUG`         | Output | `stream_channel/debug`    |

The SDK doesn't provide an API for subscribing to these channels; instead, it issues the subscription for the input
channels and for the control channel.

To publish to the analysis, control, or debug channel, call [bot_message()](#bot_message).

## Video bot command-line syntax
Video bots run from the command line. This section describes the command-line syntax and parameters.

Specify SDK options for a video bot using this syntax:
```bash
<bot_name> <RTM_options> <input_source_options> <input_control_options> <output_options> <execution_options> <pool_mode_options> <config_options> <generic_options>
```

To pass options to your own code, use the `--config-file` or the `--config` option. See the section [Config options](#config-options) for
details.

The syntax of an individual option is
`[--<option_name> | --<option_name> <value>]`

### Option types
| Option type               | Description                                                           |
|---------------------------|-----------------------------------------------------------------------|
| `<RTM_options>`           | Specify how to connect to RTM                                         |
| `<input_source_options>`  | Specify video stream source                                           |
| `<input_control_options>` | Control SDK processing of video stream                                |
| `<output_options>`        | Output messages to a file; configure Prometheus metrics server        |
| `<execution_options>`     | Control how the SDK runs the bot                                      |
| `<config_options>`        | Pass configurations to your own bot code                              |
| `<generic_options>`       | Specify output verbosity and display usage                            |

### Options by type

#### RTM options
| Option          | Value          | Type   |Description                                                                       |
|:----------------|:---------------|:------:|:---------------------------------------------------------------------------------|
| `endpoint`      | <RTM_endpoint> | string | WebSocket URL for the project that owns the bot. Get this value from Dev Portal. |
| `appkey`        | <RTM_appkey>   | string | Appkey for the project that owns the bot. Get this value from Dev Portal.        |
| `port`          | RTM port       | string | Port to use for the WebSocket connection. Defaults to `"80"`                     |

**`endpoint` and `appkey` are required. `port` is optional.**

#### Input source options
Input source option syntax:<br>
`[--input-channel <channel_name> | --input-camera | --input-video-file <video_file> | --input-replay-file <replay_file> | --input-url <url> | --input-url-parameters <parms>]`


| Option                 | Value          |  Type  | Description                                                                                                |
|:-----------------------|:--------------:|:------:|------------------------------------------------------------------------------------------------------------|
| `input-channel`        | <channel_name> | string | Name of RTM channel containing the incoming video stream                                                   |
| `input-video-file`     | <video_file>   | string | Path-relative filename of a `.mp4`, `.mkv`, or `.webm` file containing a video stream                      |
| `input-replay-file`    | <replay_file>  | string | Path-relative filename of a file containing RTM messages that represent a video stream                     |
| `input-camera`         |   -            |    -   | Tells the SDK to use a video stream from the laptop camera (macOS only)                                    |
| `input-url`            | <url>          | string | URL of a video stream source, usually a webcam                                                             |
| `input-url-parameters` | <parms>        | string |`FFmpeg` tuning parameters that the SDK encodes on the value of `input-url`. See Table note 1.              |

**Table notes**

1. To learn more, refer to the FFmpeg documentation for the **rtsp** protocol.

### Input control options
The SDK offers these options for controlling video stream processing.

| Option              | Value                            | Type    | Description                                                                                                                    |
|:--------------------|:--------------------------------:|:--------|--------------------------------------------------------------------------------------------------------------------------------|
| `loop`              |   -                              |   -     | Read all the way through the messages from the input file and start over. The SDK loops until you interrupt the bot.           |
| `input-resolution`  | `[ <width>x<height> | original]` | string  | Resolution of the input stream, in pixels. `original` tells the SDK to use original resolution recorded in the metadata.       |
| `keep-proportions`  | `[ true | false ]`               | boolean | `true` maintains the image proportions described in the metadata. `false` adjusts the proportions to the specified resolution" |
| `max-queued-frames` | number of frames                 | integer | Limits the number of video stream frames that the bot queues up for processing before it drops frames                          |

### Output options
Use these options to control output from the bot.

| Option                   | Value               | Type   |Description                                                                                                                                                               |
|:-------------------------|:-------------------:|:------:|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `analysis-file`          | <analysis_filename> | string | Path-relative name of an output file to which the SDK writes messages sent by `bot_message()` when the `bot_message_kind` argument is set to `bot_message_kind.ANALYSIS` |
| `debug-file`             | <debug_filename>    | string | Path-relative name of an output file to which the SDK writes messages sent by `bot_message()` when the `bot_message_kind` argument is set to `bot_message_kind.DEBUG`    |
| `--metrics-bind-address` | <address:port>      | string | URL and port number for the local Prometheus server that scrapes metrics from the bot                                                                                    |

| Note                                                                                                                        |
|:----------------------------------------------------------------------------------------------------------------------------|
| If you specify `analysis-file` or `debug-file`, the SDK writes messages to the file *instead of* the corresponding channel. |

### Execution options
These options control how the SDK runs the bot.

| Option         | Value                 | Type    |Description                                                                                                       |
|:---------------|:---------------------:|:-------:|------------------------------------------------------------------------------------------------------------------|
| `time-limit`   | time limit in seconds | integer |Stops the bot after the time limit is exceeded                                                                    |
| `frames-limit` | number of frames      | integer |Stops the bot after it has processed the indicated number of frames                                               |
| `batch`        |   -                   |   -     |Run the bot in batch execution mode. See [Testing with execution modes](concepts.md#testing-with-execution-modes) |

You can specify `time-limit` and `frames-limit` at the same time.

### Config options
These options control the configuration of your bot code.

| Option         | Value             | Type        | Description                                                                                                                                          |
|:---------------|:-----------------:|:-----------:|------------------------------------------------------------------------------------------------------------------------------------------------------|
| `id`           | <bot_id>          | string      | JSON-compatible string that identifies the bot. Messages you publish from the bot by calling `bot_message()` include the field `"from" : "<bot_id>"` |
| `config-file`  | <config_filename> | string      | Path-relative name of a file containing configuration information for your code. The information is a JSON object in serialized text form.           |
| `config`       | <config_JSON>     | JSON object | JSON in serialized text form, containing configuration information for your code                                                                     |

The SDK passes the serialized JSON to the `bot_ctrl_callback_t` function you provide for your bot. This is done only once,
while the SDK is initializing your bot. The JSON is passed in the `message` argument.

To learn more about the format of the JSON passed to the function, see [`bot_ctrl_callback_t`](#bot_ctrl_callback_t).

`config-file` and `config` are mutually exclusive.

Because the SDK itself uses "action" and "body" as keys in the message it passes to the `bot_ctrl_callback_t` function,
you can simplify parsing the message by avoiding the use of "action" and "body" in your own JSON.
For the same reason, avoid using the property value "configure".

### Generic options

| Option         | Value   | Type   | Description                                                                 |
|:---------------|:-------:|:------:|-----------------------------------------------------------------------------|
| `v`            | <level> | string | Amount of information that the SDK displays. See the following table.       |
| `h`            |   -     |     -  | Display SDK usage hints                                                     |


**Verbosity levels**

| Value                                               |  Meaning                                 |
|-----------------------------------------------------|------------------------------------------|
|  <center>OFF</center>                               | No information                           |
|  <center>0 or INFO</center>                         | Minimal information                      |
|  <center>1 or DEBUG</center>                        | Debug information                        |
|  <center>2 through 5</center>                       | Increasing amounts of information        |
|  <center>WARNING</center>                           | Warning, error, and fatal messages only  |
|  <center>ERROR</center>                             | Error and fatal messages only            |
|  <center>FATAL</center>                             | Fatal messages only                      |

The SDK writes the information to the standard error stream (`stderr`) for the bot.

## Command-line tools
The Video SDK command-line tools publish, record, and playback video streams.

The most important tool is `satori_video_publisher`, which publishes streaming video from a camera or file to a
Satori channel. It's the primary tool for creating the input to a video bot.

The `satori_video_recorder` tool records streaming video to a file. The source can be another video file or a camera.

To play back a video file or display camera input, use the `satori_video_player` tool.
### `satori_video_publisher`

Publish a video stream to a channel

#### `satori_video_publisher` syntax
```
satori_video_publisher \[options\] --input-video-file <vfile> --endpoint <wsendpoint> --appkey <key> --output-channel <output_channel_name> --port <wsport>
satori_video_publisher \[options\] --input-replay-file <rfile> --endpoint <wsendpoint> --appkey <key> --output-channel <output_channel_name> --port <wsport>
satori_video_publisher \[options\] --input-camera --endpoint <wsendpoint> --appkey <key> --output-channel <output_channel_name> --port <wsport>
satori_video_publisher \[options\] --input-url <URL> --endpoint <wsendpoint> --appkey <key> --output-channel <output_channel_name> --port <wsport>
```

#### `satori_video_publisher` options
```
        [--loop]
        [--output-resolution [<res>|original]]
        [--keep-proportions [true | false]]
        [--metrics-push-job     <metrics_job_value>]
        [--metrics-push-instance <metrics_instance_value>]
        [-v <verbosity>]
        [--help]
```

#### `satori_video_publisher` parameters
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

#### `satori_video_player` syntax
```
satori_video_player [options\] --endpoint <wsendpoint> --appkey <key> --port <wsport> --input-channel <input_channel_name>
satori_video_player [options\] --input-video-file <vfile>
satori_video_player [options\] --input-replay-file <rfile>
satori_video_player [options\] --input-camera
satori_video_player [options\] --input-url <URL>
```

#### `satori_video_player` options
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

#### `satori_video_player` parameters

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

Display usage hints for the utility.
