# Using Satori Video C++ SDK

Supported platforms: Mac & Linux with C++14 compiler. [Install Prerequisites](prerequisites.md)

## SDK Overview

Satori Video C++ SDK consists of:
* C++ library for writing bots processing Satori Video streams
* set of command-line utilities to produce/consume satori video streams.

The SDK is distributed as conan package with canonical name of form
`SatoriVideo/<current_version>@satorivideo/master`.

## Satori Video

Satori Video is a system of sending uncontainerized (but still codec-compressed)
video with accompanying meta-information over a set of Satori RTM channels.

Channels names are derived from `video_stream_name` the following ways:
- `video_stream_name` - channel used to send frame information
- `video_stream_name/metadata` - channel that contains codec and stream metadata
- `video_stream_name/analysis` - analysis results for the video stream
- `video_stream_name/debug` - debug messages accompanying video stream
- `video_stream_name/control` - control messages accompanying video stream


## Video Bots API, Architecture and Lifecycle

Video Bot API is declared in following files:

| File | Description |
|------|-------------|
| [video_bot.h](../include/satorivideo/video_bot.h) | full video bot api |
| [opencv_bot.h](../include/satorivideo/opencv/opencv_bot.h) | OpenCV additions to bot api |


To Video bot is defined by two callbacks:

- image callback - is used to perform processing of rendered video stream frames.
  As a result of frame processing, video bot send messages to Satori RTM with analysis
  result/debug or control messages.
- (optional) control callback - is used to configure video bot during startup
  or influence its operation while it works. If it is defined, then it is always
  called at the start with `configure` message. The content of configuration message
  can be changed by using `--config <inline_json>` or `--config-file <json_file_name>`
  bot runtime arguments.

Video Bot library currently comes in two flavors
- (default) OpenCV - image callback receives prebuilt `cv::Mat` object ready to be
  supplied to plethora of OpenCV algorithms.
- raw image buffer - image callback receives raw bytes and library has not OpenCV
  dependency. Selected by specifying `SatoriVideo:with_opencv=False` conan option.



To define video bot, define callbacks, register bot descriptor, and pass
control to video bot main function:

```c++
#include <satorivideo/opencv/opencv_bot.h>
#include <iostream>

namespace sv = satori::video;

void process_image(sv::bot_context &context, const cv::Mat &frame) {
  std::cout << "got frame " << frame.size << "\n";
}

cbor_item_t *process_command(sv::bot_context &, cbor_item_t *) {
  // will be called at least once with
  // { "action": "configure", "body": <configuration> }
  return nullptr;  // return reply control message if any.
}

int main(int argc, char *argv[]) {
  sv::opencv_bot_register({&process_image, &process_command});
  return sv::opencv_bot_main(argc, argv);
}
```


### Video Bot Commands

Whenever rtm message is received on `<video_channel>/control` channel, control callback
is executed with the message.

Also, the framework sends following control commands (command is specified by "action"
attribute  in the message):

| Action | Description |
|---------|-------------|
| `"configure"` | always sent at the start. `"body"` attribute contains configuration |
| `"shutdown"` | always sent during normal bot shutdown |


## Running Video Bots

### Choosing Video Source

Video bots support plentora of video sources: satori video, laptop camera or video file.
Run bot without parameters to see full list of available options.

| Input Type | Command Line Options |
|------------|----------------------|
| Live Satori Video | `--endpoint=<satori-endpoint>` `--appkey=<satori-appkey>` `--channel=<satori-channel>` |
| Video File | `--input-video-file=<video-file-name>` |
| Laptop Camera (Mac OSX only) | `--input-camera` |

### Common Options

Several input-related options can be used with any input type.

| Option  | Description |
|---------|-------------|
| `--time-limit=<sec>` | exit bot after specified time elapsed |
| `--frames-limit=<n>` | exit bot after specified number of frames was processed |
| `--input-resolution=<width>x<height>` | frame resolution to use for processing |
| `--keep-proportions=<1 or 0>` | specifies if proportions should be kept during frame resizing. |

### Reading Bot Configuration

The following implementation of `process_command` is expecting a JSON with string `model_file` key, for example:

```json
{"model_file": "mymodel.xml"}
```

```c++
cbor_item_t *process_command(sv::bot_context &ctx, cbor_item_t *config) {
  if (cbor::map_has_str_value(config, "action", "configure")) {
    std::string p = cbor::map(config).get_map("body").get_str("myparam", "default");
  }
  return nullptr;
}
```

This code will also perform normally on any valid JSON and without configuration at all,
in this "default" value will be assigned to `p`.

### Sending Messages

To publish a message use `bot_message` method:
```c++
void process_image(sv::bot_context &context, const sv::image_frame & /*frame*/) {
  cbor_item_t *msg = cbor_new_indefinite_map();
  cbor_map_add(
      msg, {cbor_move(cbor_build_string("msg")), cbor_move(cbor_build_string("hello"))});
  sv::bot_message(context, sv::bot_message_kind::ANALYSIS, cbor_move(msg));
}
```

There are three message kinds corresponding to different data channels and different kinds of information:

* bot_message_kind::ANALYSIS -- this kind of message is sent to "/analysis" channel and suppose to contain the result of video analysis that bot performs. For example, rectangles with detected objects, numbers for registered object kinds, motion vectors, image labels etc.
* bot_message_kind::DEBUG -- this kind of message is sent to "/debug" channel, it contains any information useful for bot diagnostics. For example, results for intermediate functions that have no interest for end-users.
* bot_message_kind::CONTROL -- this kind of message is sent to "/control" channel, it holds all the data regarding bot dynamic reconfiguration. For example, if you want to manually switch between different bot modes without bot restart.

The following message examples are given in JSON, it is made for better readability, real messages are encoded with CBOR. The coordinates are specified as proportions to full image size to solve the problem of different bots using different image resolutions.

Detected rectangles with objects:
```json
{"objects": [{"tag": "car", "rect": [0.225, 0.2167, 0.0625, 0.0584]}]}
```
in this example each object has rectangular area set with `[x, y, width, height]` array.

Numbers of registered object kinds:
```json
{"counts": [{"car": 8, "truck": 1}]}
```

Detected motion vector:
```json
{"motion": [{"start": [0.225, 0.2167], "end": [0.2367, 0.3673]}]}
```
in this example `start` and `end` are arrays of `[x,y]`.

Recognized image categories:
```json
{"labels": [{"text": "horse", "probability": 0.2332}]}
```

Debug message:
```json
{"message": "Tracking failed", "rect": [0.225, 0.2167, 0.0625, 0.0584]}
```
Indicates that a bot has lost some object in a given rectangular area.

Control message:
```json
{"set_threshold" : 67}
```
Setting different level for image binarization.

All the messages are bot-specific and depend on the algorithm that a bot implements.

## Executing Video Bots

Video bots support plethora of video sources: satori video, laptop camera or video file.
Run bot without parameters to see available options.


## Deploying Video Bots to Cloud

Satori Video Bot library is a static C++ library. We recommend full static linking
of your bot binaries and using Docker to deploy them to the cloud.

## Debugging and Profiling Video Bots
Video bots can be debugged/profiled as any normal C++ process.
We recommend following tools: gdb, lldb, (Mac) Instruments, perf, CLion.

To simplify debugging in production the bot library links with gperftools' tcmalloc
and profiler. This can be disabled by specifying `SatoriVideo:with_gperftools=False`.

## SDK Command Line Tools

| Name   | Description |
|--------|-------------|
| `satori_video_player` | Allows watching video files, Satori video streams, and grab video from laptop camera (Mac OSX only) |
| `satori_video_publisher` | Allows creating Satori video streams from video files or laptop camera (Mac OSX only), so other clients can subscribe to them |
| `satori_video_recorder` | Allows recording Satori video streams into video files. [Matroska](http://matroska.org/) is the only supported video file format |

### Running Player SDK CLI tool
```shell
# Watch Satori video stream
./satori_video_player --endpoint=<satori-endpoint> \
                      --appkey=<satori-appkey> \
                      --channel=<satori-channel>

# Watch video file
./satori_video_player --input-video-file=my_video_file.mp4

# Watch video stream from laptop camera (Mac OSX only)
./satori_video_player --input-camera
```

### Running Publisher SDK CLI tool
```shell
# Create Satori video stream from video file
./satori_video_publisher --input-video-file=my_video_file.mp4 \
                         --endpoint=<satori-endpoint> \
                         --appkey=<satori-appkey> \
                         --channel=<satori-channel>

# Create Satori video stream from laptop camera
./satori_video_publisher --input-camera \
                         --endpoint=<satori-endpoint> \
                         --appkey=<satori-appkey> \
                         --channel=<satori-channel>
```

### Running Recorder SDK CLI tool
```shell
# Record Satori video stream into file
./satori_video_recorder --output-video-file=my_video_file.mkv \
                        --endpoint=<satori-endpoint> \
                        --appkey=<satori-appkey> \
                        --channel=<satori-channel>
```
