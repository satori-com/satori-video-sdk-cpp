# Using Satori Video C++ SDK

Supported platforms: Mac & Linux with C++14 compiler. [Install Prerequisites](docs/prerequisites.md)

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


## Video Bots Architecture and Lifecycle

Video bot is defined by two callbacks:

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
  supplied to plentora of OpenCV algorithms.
- raw image buffer - image callback receives raw bytes and library has not OpenCV
  dependency. Selected by specifying `SatoriVideo:with_opencv=False` conan option.

To define video bot, define callbacaks, register bot descriptor, and pass 
control to video bot main function:

```
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

## Executing Video Bots

Video bots support plentora of video sources: satori video, laptop camera or video file. 
Run bot without parameters to see available options.

## SDK Command Line Tools

## Deploying Video Bots to Cloud

Satori Video Bot library is a static C++ library. We recommend full static linking
of your bot binaries and using Docker to deploy them to the cloud.

## Debugging and Profiling Video Bots
Video bots can be debugged/profiled as any normal C++ process. 
We recommend following tools: gdb, lldb, (Mac) Instruments, perf, CLion.

To simplify debugging in production the bot library links with gperftools' tcmalloc
and profiler. This can be disabled by specifying `SatoriVideo:with_gperftools=False`.

