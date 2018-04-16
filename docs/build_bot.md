# Satori Video SDK for C++: Build and Deploy a Video Bot

[All Video SDK documentation](../README.md)

## Table of contents
* [Set up files](#set-up-files)
    * [Install the prerequisites](#install-the-prerequisites)
    * [Clone the video bot example repository](#clone-the-video-bot-example-repository)
    * [Choose a video bot template](#choose-a-video-bot-template)
    * [Make a copy](#make-a-copy)
    * [Write your image processing program](#write-your-image-processing-program)
    * [Set up conan](#set-up-conan)
* [Build the bot](#build-the-bot)
* [Test the bot](#test-the-bot)
* [Test the SDK tools](#test-the-sdk-tools)
* [Deploy the bot](#deploy-the-bot)

## Overview
To create a video bot, use `conan` to install the video SDK and dependencies. Then use `cmake` to build and link your code
together with the Video SDK libraries. The result is a statically-linked C++ program that you can deploy in a variety of
environments.

`conan` ensures that the program is built with the tools and dependencies that match your platform.

The Video SDK provides bot templates in the
[Satori Video SDK Examples](https://github.com/satori-com/satori-video-sdk-cpp-examples) GitHub repository
Each template includes a skeleton C++ program, as well as `conan` and `cmake` files.

The SDK source is also available. You don't need it unless you want to modify a part of
the SDK. The instructions for building and modifying the SDK source are listed in the topic
[Contributing to the Satori Video SDK for C++ project](contributing.md).

## Set up files
Start by setting up the prerequisites and video bot framework.

### Install the prerequisites
See [Satori Video SDK for C++ Prerequisites](prerequisites.md).

### Clone the video bot example repository
See the [Satori Video SDK Examples](https://github.com/satori-com/satori-video-sdk-cpp-examples) GitHub repository.

```
$ git clone https://github.com/satori-com/satori-video-sdk-cpp-examples.git
```

### Choose a video bot template

Choose the starting template for your video bot from the templates in the **Satori Video C++ SDK Examples** repository.
See [Template video bots](reference.md#template-video-bots) to learn how to obtain the templates.

### Make a copy

Make a copy of the following directories and files from the video bot template you choose:
```
src/
CMakeLists.txt
Dockerfile
Makefile
conanfile.txt
```

**Note:** If you're not using Docker, you don't need to copy `Dockerfile`.

### Write your image processing program
Provide code for the following functions (you can change their names if desired):

* `process_image()`: The SDK framework invokes this callback and passes it the current video frame.
* `process_command()`: The SDK framework invokes this callback during initialization and when it
receives a message in the control channel.

Update the call to `bot_register()` to refer to the names of the functions you wrote.

### Set up `conan`
Edit `conanfiles.txt`. In the `[generators]` section, add the generator `virtualenv`.

This tells conan to add two convenience scripts, `activate.sh` and `deactivate.sh`, during setup. These scripts
modify your environment variables to give you access to the SDK command line tools. After you run `cmake` to
setup up your build files, the scripts are in the `build` subdirectory of your project.

Using these scripts means that you don't have to modify your `.bash_profile` or `.bashrc`.

After you have the scripts, get access to the SDK command line tools by entering the following:

```
$ source activate.sh
```

This modifies your PATH to include the location of the tools in your conan local cache. To switch back to your
normal configuration, enter:
```
$ source deactivate.sh
```

## Build the bot
1. In your project directory `<projectdir>`, create a `build` directory and then navigate to it:
```
$ mkdir build && cd build
```

2. Run `cmake` to set up the build files
```
cmake -DCMAKE_BUILD_TYPE=Release ../
```

3. Run `make` to build the bot. The `-j 8` cores parameter is optional; you can use as many cores as you want.
```
make -j 8
```
## Test the bot

To test your bot, navigate to the `build/bin` directory in `<projectdir>`, and run the executable file without any
parameters. For example, if you used the `empty-bot` template, enter
```
$ empty_bot
```
If the bot built successfully, it displays usage information for video bots.

## Test the SDK tools

To test the SDK command line tools, first activate the special environment. Navigate to the `build` directory, then
enter:
```
$ source activate.sh
```

This modifies your PATH variable to include the location of the SDK tools. In addition, to remind you that you have
a special environment set, it prepends the name of the template directory to your prompt.

After you activate the environment, run one of the SDK tools without any parameters. For example:

```
$ sdk_video_publisher
```

This should display usage information for the tool.

Remember to deactivate the special environment when you're finished.

## Deploy the bot
You can now deploy the bot in an environment of your choice. The bot executable is statically-linked.
