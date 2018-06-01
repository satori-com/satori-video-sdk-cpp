# Satori Video SDK for C++: Message formats

[All Video SDK documentation](../README.md)

## Table of contents
* [Fields added by the SDK](#json-fields-added-by-the-sdk)
* [Analysis messages](#analysis-messages)
* [Debug messages](#debug-messages)
* [Configuration messages](#configuration-messages)

## Overview
Video bots use the Satori publish-subscribe platform for communication, so input and output take
the form of JSON messages:
* To send analysis and debug output, you publish a message with the video API function `bot_message()`.
* You receive configuration input in your command processing callback. You can also publish messages
from this function.
* The SDK provides metrics to a push server that you can link to your own Prometheus site.
* The SDK ingests video stream and video meta-data messages.

You can use any JSON content you want to publish analysis and debug messages from your bot. You can also use any format
you want to publish configuration messages to the control channel.

## JSON fields added by the SDK
The SDK adds two fields to messages you publish using `bot_message()` or return from your command processing function:
* `"from": "<bot_id>"`: An identifier for the bot that you specify on the bot program command line with the `--id` parameter.
If you don't specify the parameter, the SDK doesn't add this field.
* `"i": [frame1,frame2]`: Frame identifier, defined by the SDK API struct [`frame_id`](reference.md#frame_id). The
field has two 64-bit integers containing frame numbers `frame1` and `frame2`. If `frame1==frame2`, the id specifies a
single frame; otherwise, it specifies a range of frames from `frame1` to `frame2`, inclusive. The default value is
frame{[0,0]}, which the SDK interprets as the current frame.

**Notes:**
* `bot_id`: The SDK inserts the exact value you specify on the command line.
* `frame_id`: When the SDK invokes `process_image()`, it passes in the current frame as an
[`image_frame` struct](reference.md#image_frame). This struct has a `frame_id` member that contains the id of the
frame.

## Analysis messages
To publish an analysis message containing the results of your image processing, call
[`bot_message()`](reference.md#bot_message) with the `kind` parameter set to the SDK API `enum` constant
`bot_message_kind::ANALYSIS`.

## Debug messages
To publish a debug message, call [`bot_message()`](reference.md#bot_message) with the `kind` parameter set to the
SDK API `enum` constant `bot_message_kind::DEBUG`.

# Configuration messages
The SDK passes messages it receives in the control channel to the command processing callback function you
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
`instance_data` member of [`bot_context`](reference.md#bot_context) to make them available to your image processing
callback function.
