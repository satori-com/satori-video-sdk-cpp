# Contributing to the Satori Video SDK for C++ project

[All Video SDK documentation](../README.md)

## Table of contents
* [Prerequisites](#prerequisites)
* [Clone the SDK](#clone-the-sdk)
* [Build the SDK](#build-the-sdk)
* [Test SDK modifications](#test-sdk-modifications)

## Prerequisites
1. Set up the prerequisites described in the topic [Satori Video SDK for C++ Prerequisites](prerequisites.md).
2. At a command prompt, enter `make -version`. Ensure that your version is GNU make version 4.2.1 or later.
3. At a command prompt, enter `make conan-login` to ensure that you can access conan remote repositories.
4.

## Clone the SDK
```shell
$ git clone https://github.com/satori-com/satori-video-sdk-cpp.git
```

## Build the SDK
The first build requires more time than subsequent builds, because the `conan` package manager
needs to retrieve and build dependencies.

If you encounter errors during the build, try removing the `build` directory and re-building.

```shell
$ cd satori-video-sdk-cpp
$ mkdir build 
$ cd build 
$ cmake ../
$ make -j 8
```

## Test SDK modifications

### Run the automated tests
```bash
$ ctest
```

### Test the SDK from local conan
Export `satori-video-sdk-cpp` to your local conan cache:

```shell
$conan export satori-video-sdk-cpp <user>/<channel>
```

This creates a test version of the SDK in `<conan_dir>/.data/SatoriVideo/<version>/<user>/<channel>`.

* **&lt;version&gt;** - The latest released version of the SDK in the conan remote cache
* **&lt;user&gt;** - A value you assign, usually your OS user id
* **&lt;channel&gt;** - A value you assign

### Test SDK with example bot

1. Set up a test bot from the SDK examples or use one of your own working video bots:
    * **Test bot from SDK examples:** Follow the instructions in the topic
      [Satori Video SDK for C++: Build and Deploy a Video Bot](build_bot.md) in the section
      [Set up files](build_bot.md#set-up-files) to set up a test bot.
    * **Your own working bot:** Make a copy of the directory that contains your working bot.

2. Before you build the bot, modify `conanfile.txt`. In the `[requires]` section, modify the line that specifies the
video SDK. Change the following line:<br><br>
`SatoriVideo/[<version_spec>]@satorivideo/master`<br><br>
to<br><br>
`SatoriVideo/[<version_spec>]@<user>/<channel>`<br><br>
where `<user>` and `<channel>` are the values you specified in [Export SDK to local conan](#export-sdk-to-local-conan).<br><br>
When you build the bot, this modification directs `conan` to use the test version of your SDK.

3. Build your bot and test it.

### Build with ASAN memory error detection:
`$ cmake -DCMAKE_CXX_SANITIZER="address" ..`

### Build with `clang-tidy`:
`$ cmake -DCMAKE_CXX_CLANG_TIDY="/usr/local/opt/llvm/bin/clang-tidy .."`
