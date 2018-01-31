# Satori Video SDK for C++ Prerequisites

[All Video SDK documentation](../README.md)

The video SDK has these groups of prerequisites:

* **Platform:** Hardware and OS requirements to build and run a video bot
* **Tools:** Tools you need to install before you build a video bot
* **Optional:** Tools and libraries you need to install to use optional systems.
* **SDK dependencies:** Libraries that the SDK libraries depend on. The conan package manager downloads and
installs these dependencies the first time you build a bot.

## Platform prerequisites
* macOS or Linux
* C++14 compiler

## Tools prerequisites

### macOS:
* conan
* cmake
* autoconf
* libtool
* gnu make, version >=3.82

**Note:**
* Use `brew install`, except for gnu make
* For gnu make, use `brew install --with-default-names` so that `brew`
installs the tool as `make` rather than `gnu-make`.

### Linux:

* build-essential
* clang
* cmake
* git
* ninja-build
* python-pip
* yasm
* zlib1g-dev
* clang-tidy
* autoconf
* libtool
* curl
* conan, version >=0.28.0
* wget

**Note:**

* Use `sudo apt-get install -y`, except for conan
* For conan, use `sudo pip install 'conan>=0.28.0'`

### conan setup

Specify the conan remote server:

```shell
$ conan remote update video-conan 'http://video-conan.api.satori.com'
```

### SDK dependencies
Installed automatically by conan. Because the build process links them into the video bot, you
can use them in your program:

* libcbor: CBOR utilities (only the bot framework requires this library, but you can use it if you want)
* loguru: Loguru logging system
* opencv: OpenCV libraries
* libprometheus-cpp: Prometheus metrics library
* json

