# Satori Video C++ SDK Overview

:warning: **PRIVATE: NOT FOR RELEASE***

:construction: Documentation is under construction :construction:

The Satori Video SDK processes compressed streaming video. It provides a bot framework that decodes video into frames.
The framework passes the frames to image processing code that you provide. In turn, your code analyzes or
transforms the video data. A C++ API lets you publish image processing results, debug messages, and metrics from the
bot. The API also lets you receive messages containing configuration information.

The SDK uses the [Satori](https://www.satori.com/docs/introduction/new-to-satori) publish/subscribe platform, which
provides reliable, ultra-high-speed, fast throughput I/O. It accepts real-time streaming video and can
publish image processing results to thousands of subscribers at once.

The Video SDK works with macOS and Linux. See [Satori Video SDK for C++ Prerequisites](docs/prerequisites.md)
for more information.

Documents marked with a :construction: are not ready for review
## Documentation
| Document                                                                   | Contents                                |
|----------------------------------------------------------------------------|-----------------------------------------|
[Satori Video SDK for C++ Concepts](docs/concepts.md)                        | Overview of the SDK                     |
[Satori Video SDK for C++ Prerequisites](docs/prerequisites.md)              | Prerequisites to use the SDK            |
[Satori Video SDK for C++ Tasks](docs/tasks.md)                              | Common SDK procedures                   |
[Satori Video SDK for C++ Reference](docs/reference.md)                      | SDK reference                           |
[Satori Video SDK for C++: Build and Deploy a Video Bot](docs/build_bot.md)  | How to build a video bot                |
[Satori Video SDK for C++: Message Formats](docs/message_formats.md)         | Message formats reference               |
[Contributing to the Satori Video SDK for C++ project](docs/contributing.md) | How to contribute                       |
