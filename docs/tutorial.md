# Satori Video C++ SDK Tutorial

Supported platforms: Mac, Linux. [Install Prerequisites](docs/prerequisites.md)

## Building First Video Bot

Clone [examples project](https://github.com/satori-com/satori-video-sdk-cpp-examples):

```
git clone git@github.com:satori-com/satori-video-sdk-cpp.git
cd satori-video-sdk-cpp
```

Build a empty-bot:

```
cd empty-bot
mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release ../ && make -j8
```

## Run Video Bot

Run bot without arguments to see list of available inputs.

Examples:

Run video bot with video file:

```
./empty-bot --input-video-file=my_video_file.mp4
```


TODO RTM/Camera/RTM sources

## Add Processing Code

## Deploy Video Bot
