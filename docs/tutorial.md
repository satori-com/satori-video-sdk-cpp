# Satori Video C++ SDK Tutorial

Supported platforms: Mac & Linux with C++14 compiler. [Install Prerequisites](docs/prerequisites.md)

## Building First Video Bot

Clone [examples project](https://github.com/satori-com/satori-video-sdk-cpp-examples):

```
git clone git@github.com:satori-com/satori-video-sdk-cpp.git
cd satori-video-sdk-cpp
```

Build an empty-opencv-bot (or empty-bot if you prefer to work without opencv wrapper):

```
cd empty-opencv-bot
mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release ../ && make -j8
```

First time build will take significant time while conan fetches and builds required packages.


## Running Video Bot

Run bot without arguments to see list of available inputs.

Examples:

Run video bot with video file:

```shell
./empty-bot --input-video-file=my_video_file.mp4
```

Run video bot with Satori video stream:
```shell
./empty-bot --endpoint=<satori-endpoint> --appkey=<satori-appkey> --channel=<satori-channel>
```

## SDK CLI tools

| Name   | Description |
|--------|-------------|
| `satori_video_player` | Allows watching video files, Satori video streams, and grab video from laptop camera (Mac OSX only) |
| `satori_video_publisher` | Allows creating Satori video streams from video files or laptop camera (Mac OSX only), so other clients can subscribe to them |
| `satori_video_recorder` | Allows recording Satori video streams into video files. [Matroska](http://matroska.org/) is the only supported video file format |

### Running Player SDK CLI tool
```shell
# Watch Satori video stream
./satori_video_player --endpoint=<satori-endpoint> --appkey=<satori-appkey> --channel=<satori-channel>

# Watch video file
./satori_video_player --input-video-file=my_video_file.mp4

# Watch video stream from laptop camera (Mac OSX only)
./satori_video_player --input-camera
```

### Running Publisher SDK CLI tool
```shell
# Create Satori video stream from video file
./satori_video_publisher --input-video-file=my_video_file.mp4 --endpoint=<satori-endpoint> --appkey=<satori-appkey> --channel=<satori-channel>

# Create Satori video stream from laptop camera
./satori_video_publisher --input-camera --endpoint=<satori-endpoint> --appkey=<satori-appkey> --channel=<satori-channel>
```

### Running Recorder SDK CLI tool
```shell
# Record Satori video stream into file
./satori_video_recorder --output-video-file=my_video_file.mkv --endpoint=<satori-endpoint> --appkey=<satori-appkey> --channel=<satori-channel>
```

## Add Processing Code

## Deploy Video Bot
