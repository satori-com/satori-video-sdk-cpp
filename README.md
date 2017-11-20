# Satori Video C++ SDK

The Satori Video SDK is a library of APIs for building bots that analyze compressed, streaming video in the form of messages.
The SDK subscribes to a channel containing streaming video messages, continuously receive these messages, decompress
them, and convert them to individual image frames. To analyze these frames, you provide the SDK with an image
processing callback function that's invoked for each new frame. In this callback, you analyze the frames using your own
or 3rd-party libraries and publish results via an API call that uses the Satori publish-subscribe platform. This API
call can also publish debug and metrics messages to their own channels.

The SDK uses the [Satori](https://www.satori.com/docs/introduction/new-to-satori) publish-subscribe platform, which
provides reliable, high throughput I/O. It accepts real-time streaming video and can publish image processing results to
thousands of subscribers at once.

The Video SDK works with macOS and Linux. See [Satori Video SDK for C++ Prerequisites](docs/prerequisites.md)
for more information.

## Documentation
| Document                                                                   | Contents                                |
|----------------------------------------------------------------------------|-----------------------------------------|
[Satori Video SDK for C++ Concepts](docs/concepts.md)                        | Overview of the SDK                     |
[Satori Video SDK for C++ Prerequisites](docs/prerequisites.md)              | Prerequisites to using the SDK          |
[Satori Video SDK for C++ Tasks](docs/tasks.md)                              | Common tasks                            |
[Satori Video SDK for C++ Reference](docs/reference.md)                      | SDK reference                           |
[Satori Video SDK for C++: Tutorial](docs/tutorial.md)                       | SDK tutorial
[Satori Video SDK for C++: Build and Deploy a Video Bot](docs/build_bot.md)  | How to build a video bot                |
[Satori Video SDK for C++: Message Formats](docs/message_formats.md)         | Message formats reference               |
[Contributing to the Satori Video SDK for C++ project](docs/contributing.md) | How to contribute to the project        |
