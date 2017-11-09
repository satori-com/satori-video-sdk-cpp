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

Check out [Running Video Bots](using_sdk.md#running-video-bots) section in user guide for more ways to configure
bot input or tweak its running parameters.

## Add Processing Code

## Deploy Video Bot
