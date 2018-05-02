# Satori Video SDK for C++ Tasks

[All Video SDK documentation](../README.md)

## Table of contents
* [Configure a bot](#configure-a-bot)
* [Debug and profile a bot](#debug-and-profile-a-bot)
* [Publish messages from a bot](#publish-messages-from-a-bot)
* [Work with video streams](#work-with-video-streams)
    * [Watch a video stream](#watch-a-video-stream)
    * [Watch a video file](#watch-a-video-file)
    * [Watch video from a camera](#watch-video-from-a-camera)
* [Publish video](#publish-video)
    * [Publish video from a video file](#publish-video-from-a-video-file)
    * [Publish video from a macOS laptop camera](#publish-video-from-a-macos-laptop-camera)
* [Record video](#record-video)
    * [Record a video stream to a file](#record-a-video-stream-to-a-file)

## Configure a bot

### Command-line configuration
The SDK passes the JSON specified by the `--config` option or the contents
of the JSON file specified by the `--config-file` option to your configuration callback. The information is in the
`message` parameter of your callback in JSON format.

To use the `--config-file` paramter:

```shell
$ my_bot [other parameters] --config-file <configfile>
```

In your configuration file `<configfile>`, add this JSON:

```json
{
    "<param_key>": <"param_value">
    [,"<param_key>": <"param_value">]
}
```

To use the `--config` with the same configuration:

`$ my_bot [other parameters] --config '{ "<param_key>": <"param_value"> [,"<param_key>": <"param_value">] }'`

### Dynamic configuration
To pass configuration parameters to your bot as it runs, publish a message to the control channel
(see [SDK channel names](reference.md#sdk-channel-names)). The message must have the proper format (see
[bot_ctrl_callback_t](reference.md#bot_ctrl_callback_t)); otherwise, the bot aborts.

The SDK invokes your configuration callback function when it receives a control channel message. The function can
also return a JSON value to acknowledge the message. This is also documented in
[bot_ctrl_callback_t](reference.md#bot_ctrl_callback_t).

The [Satori Video SDK for C++: Tutorial](tutorial.md) contains an example of receiving and acknowledging a control
channel message.

## Debug and profile a bot

You can debug and profile video bots as normal C++ processes.

Use the following tools:
* `gdb`
* `lldb`
* Instruments (macOS only)
* `perf`
* CLion

To simplify debugging in production, the bot library links with gperftools' tcmalloc
and profiler. To disable `gperftools`, you need to re-build your bot:
1. In the root of your bot project, edit the conan package script `conanfile.txt`.
2. Find the `[options]` section.
3. Add the line `SatoriVideo:with_gperftools=False`.
4. Re-build your bot. See [Build the bot](build_bot.md#build-the-bot).

## Publish messages from a bot

To publish a message from a bot, call [`bot_message()`](reference.md#bot_message).

The `bot_message_kind` parameter is an enumeration that specifies the destination channel. See
[SDK channel names](reference.md#sdk-channel-names) for a list of the `enum` constants and corresponding
channel names for `bot_message_kind`.

For example, in your image processing callback you can publish a message to the analytics channel:

```c++
namespace sv = satori::video;

namespace empty_bot {
void process_image(sv::bot_context &context, const sv::image_frame & /*frame*/) {
    std::cout << "got frame " << context.frame_metadata->width << "x"
            << context.frame_metadata->height << "\n";
    sv::bot_message(context, sv::bot_message_kind::ANALYSIS, {{"msg", "hello"}});
}

}
```

Don't publish messages to your own custom channels unless absolutely necessary. If you have an important reason
to publish to your own channel, use the [Satori C RTM SDK](https://www.satori.com/docs/rtm-sdks/tutorials/c-sdk-quickstart).

## Work with video streams
When you build your video bot, the conan package recipe installs the command-line utilities to your local
cache. If you modify the `conanfile.txt` for your project to include the `virtualenv` generator, the build
creates environment management scripts called `activate.sh` and `deactive.sh`:
* `$ <project_dir>/source activate.sh` changes your environment to add the command-line utilities to your path.
* `$ <project_dir>/source deactive.sh` change your environment back to its previous settings. To learn more, see
[Test the SDK utilities](build_bot.md#test-the-sdk-utilities).

### Watch a video stream
Given the following:
* endpoint: `xxxxxxx.api.satori.com`
* appkey: `00112233445566778899AABBCCDDEEFF`
* channel: `input_channel`

The following shell commands play video from the channel in a GUI window:

```shell
$ cd <project_dir>
$ source activate.sh
$ ./satori_video_player --endpoint="xxxxxxx.api.satori.com" \
                        --appkey="00112233445566778899AABBCCDDEEFF" \
                        --input-channel="input_channel"
```

Remember to run `$ source deactivate.sh` when you're done.
### Watch a video file
The following shell commands play a video file in a GUI window:

```shell
$ cd <project_dir>
$ source activate.sh
$ ./satori_video_player --input-video-file=my_video_file.mp4
```
Remember to run `$ source deactivate.sh` when you're done.

### Watch video from a camera
**Note:** This option is only available for a macOS laptop:

The following shell commands display video from your laptop camera:

```shell
$ cd <project_dir>
$ source activate.sh
$ ./satori_video_player --input-camera
```
Remember to run `$ source deactivate.sh` when you're done.
## Publish video

### Publish video from a video file
Given the following:
* endpoint: `xxxxxxx.api.satori.com`
* appkey: `00112233445566778899AABBCCDDEEFF`
* channel: `input_channel`

The following shell commands publish video from a file to the channel:

```shell
$ cd <project_dir>
$ source activate.sh
$ ./satori_video_publisher --input-video-file=my_video_file.mp4 \
                           --endpoint="xxxxxxx.api.satori.com" \
                           --appkey="00112233445566778899AABBCCDDEEFF" \
                           --input-channel="input_channel"
```
Remember to run `$ source deactivate.sh` when you're done.

### Publish video from a macOS laptop camera
Given the following:
* endpoint: `xxxxxxx.api.satori.com`
* appkey: `00112233445566778899AABBCCDDEEFF`
* channel: `input_channel`

The following shell commands publish video from a file to the channel:
```shell
$ cd <project_dir>
$ source activate.sh
$ ./satori_video_publisher --input-camera \
                           --endpoint="xxxxxxx.api.satori.com" \
                           --appkey="00112233445566778899AABBCCDDEEFF" \
                           --channel="input_channel"
```
Remember to run `$ source deactivate.sh` when you're done.

## Record video

### Record a video stream to a file
Record a video stream, optionally with time and frame limits.

Given the following:
* endpoint: `xxxxxxx.api.satori.com`
* appkey: `00112233445566778899AABBCCDDEEFF`
* channel: `input_channel`

```shell
$ cd <project_dir>
$ source activate.sh
$./satori_video_recorder --output-video-file=my_video_file.mkv \
                        --endpoint="xxxxxxx.api.satori.com" \
                        --appkey="00112233445566778899AABBCCDDEEFF" \
                        --channel="input_channel"
```

* To limit the recording to a certain number of seconds, add the parameter `--time-limit`. For example,
`time-limit 30` limits the video to 30 seconds.
* To limit the recording to a certain number of video frames, add the parameter `--frame-limit`. For example,
`frame-limit 1000` limits the video to 1000 frames.

Remember to run `$ source deactivate.sh` when you're done.
