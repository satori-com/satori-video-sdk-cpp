# Satori Video SDK for C++ Prerequisites

[All Video SDK documentation](../README.md)

## Table of contents
* [Platform prerequisites](#platform-prerequisites)
* [Tools prerequisites](#tools-prerequisites)
    * [macOS](#macos)
    * [Linux](#linux)
    * [conan setup](#conan-setup)
    * [SDK dependencies](#sdk-dependencies)
* [Satori prerequisites](#satori-prerequisites)

## Overview
The video SDK has these groups of prerequisites:

* **Platform:** Hardware and OS requirements to build and run a video bot
* **Tools:** Tools you need to install before you build a video bot
* **Optional:** Tools and libraries you need to install to use optional systems.
* **SDK dependencies:** Libraries that the SDK libraries depend on. The conan package manager downloads and
installs these dependencies the first time you build a bot.

## Platform prerequisites
* macOS or Linux
* C++14 compiler
    * macOS: clang or gcc
    * Linux: clang

## Tools prerequisites

### macOS:
Install the prerequisites using `brew`:
* `brew install Ninja`
* `brew install automake`
* `$brew install conan`
* `$brew install cmake`
* `$brew install autoconf`
* `$brew install libtool`
* `$brew install nasm`
* `$brew install yasm`
* `$brew install make --with-default-names`



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

Set up the conan remote server.

**Note:** If you previously added the video SDK server, the `remote add` command may return the message:<br>
`ERROR: Remote 'video-conan' already exists in remotes (use update to modify)`<br>
You can ignore this error. **Always** run `conan remote update`.

First run
```shell
$ conan remote add video-conan 'http://video-conan.api.satori.com'
```

**Note:** When you first run `conan`, it creates a registry file and displays the message:
```bash
WARN: Remotes registry file missing, creating default one in /<user_path>/.conan/registry.txt
```

Next, run
```
$ conan remote update video-conan 'http://video-conan.api.satori.com'
```

### SDK dependencies
Installed automatically by conan. Because the build process links them into the video bot, you
can use them in your program:

* libcbor: CBOR utilities (only the SDK itself requires this library, but you can use it if you want)
* loguru: Loguru logging system
* opencv: OpenCV libraries
* libprometheus-cpp: Prometheus metrics library
* json

## Satori prerequisites
A video bot uses Satori publish-subscribe channels to receive streaming video and publish results and other messages.
To access these channels, the bot needs credentials that you get from the Satori Dev Portal.

**To get channel credentials:**

1. Navigate to the [Satori Dev Portal](https://developer.satori.com).
2. If you don't already have a Satori account, sign up for one.
3. When you have a Satori account, log in to the Dev Portal.
4. To create a new project for your video bot, select **Start a project**:
   1. Enter a name for the project and click **Start a project**:
   2. Copy the appkey and endpoint for the project. You need to provide these credentials whenever you access a Satori channel.
   3. If you want fine-grained channel control, create user roles and channel permissions. To learn more, see
      the topic [Access Control](https://www.satori.com/docs/using-satori/authentication) in *Satori Docs*.
   4. Ignore the Streambots™ settings. Video bots don't use Streambot™ technology.
5. Click **Save** and exit the Dev Portal.
