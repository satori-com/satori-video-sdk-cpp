# Satori Video SDK for C++: Message formats

[All Video SDK documentation](../README.md)

## Table of contents
* [Detected objects](#detected-objects)
* [Counted objects](#counted-objects)
* [Label messages](#label-messages)
* [Debug message](#debug-message)
* [Configuration message](#configuration message)

## Overview
Video bots use the Satori publish-subscribe platform for communication, so input and output take
the form of JSON messages:
* To send analysis and debug output, you publish a message with the video API function `bot_message()`.
* You receive configuration input in the callback function `process_command()`. You can also publish messages
from this function.
* The API publishes to the metrics channel the Prometheus metrics you store in the bot context metrics registry.
* The API itself ingests video stream and video meta-data messages.

You can use any format you want to publish analysis and debug messages from your bot. You can also use any format
you want to publish configuration messages to the control channel.

## API fields
The API adds can add two fields to messages you publish using `bot_message()` or `process_command()`:
* `"from": "<bot_id>"`: An identifier for the bot that you specify on the bot program command line with the `--id` parameter.
If you don't specify the parameter, the API doesn't add this field.
* `"i": [frame1,frame2]`: Frame identifier, defined by the API struct [`frame_id`](reference.md#frame-id). The
field has two 64-bit integers containing frame numbers `frame1` and `frame2`. If `frame1==frame2`, the id specifies a
single frame; otherwise, it specifies a range of frames from `frame1` to `frame2`, inclusive. The default value is
frame{[0,0]}, which the API interprets as the current frame.

**Notes:**
* `bot_id`: The API inserts the exact value you specify on the command line.
* `frame_id`: When the API invokes `process_image()`, it passes in the current frame as an
[`image_frame` struct](reference.md#image-frame). This struct has a `frame_id` member that contains the id of the
frame.

## Analysis messages
To publish an analysis message containing the results of `process_image()`, call
[`bot_message()`](reference.md#bot-message) with the `kind` parameter set to the API `enum` constant
`bot_message_kind::ANALYSIS`.

## Debug messages
To publish a debug message, call [`bot_message()`](reference.md#bot-message) with the `kind` parameter set to the
API `enum` constant `bot_message_kind::DEBUG`.

## Configuration messages
The API passes messages it receives in the control channel to the `process_command()` callback function you
define, using the parameter `message`. This parameter is specified as a JSON object.

The following JSON object is an example of a configuration used to set the feature size in a motion detector bot:
```json
{
    "params": {
      "featureSize" : 7.0
    }
}
```
This message provides a threshold value for detecting objects.

When you receive updated configurations from the control channel, move the values to the
`instance_data` member of [`bot_context`](reference.md#bot-context) to make them available to your `process_image()`
callback function.
