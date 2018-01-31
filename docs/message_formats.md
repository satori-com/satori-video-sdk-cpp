# Satori Video SDK for C++: Message formats

[All Video SDK documentation](../README.md)

Satori's proprietary image processing software publishes analytics messages in standard formats that other Satori
systems use. These messages use simple JSON-formatted structures. Although you can use any format you want, using the
Video SDK formats helps your bot connect with Satori systems.

To publish an analytics message from a video bot, call `bot_message()` with the `kind` parameter set to `ANALYSIS`. For
example:
```c++
bot_message(context, bot_message_kind::ANALYSIS, result_message);
```

To learn more about `bot_message()` see [Publish messages from a bot](tasks.md#publish-messages-from-a-bot).

### Detected objects
A `detected_objects` message contains information about objects detected in the specified frames. In the Satori
system, a detected object is something in the frame that the image processing algorithm matched to an object of interest.
The message is simply a record that the object is found in the frame.

The following JSON object is an example of a `detected_object` message you might publish with `bot_message`:
```json
{
    "detected_objects" : [
        {
            "rect": [0.147, 0.208, 0.147, 0.146],
            "id" : "motorcycle",
            "tag" : "vehicle",
            "color" : "yellow"
        },
        {
            "rect": [0.109, 0.108, 0.127, 0.150],
            "id" : "car",
            "tag" : "vehicle",
            "color" : "yellow"
        }
    ]
}
```

|     Entity                                       |   Value type       |  Description
| :----------------------------------------------- | :----------------: | :------------------------------------------------------------------------------------------------------------ |
 `"detected_objects"`                              |  string            | Handle this data as detected objects                                                                          |
 (list of objects)                                 |  object\[\]        | A list of the objects detected                                                                                |
 (object)                                          |  object            | Attributes of each detected object                                                                            |
 `"rect"`:`[x, y, width, height]`                  |  array of number   | Dimensions of the rectangle that encloses an object format                                                    |
 `id`:\<identifier>                                |  string            | **Optional** identifier that's specific to this object                                                        |
 `tag`:\<tag>                                      |  string            | **Optional** tag for this type of object                                                                      |
 `color`:\<color>                                  |  string            | **Optional** color to use for the enclosing rectangle when displayed                                          |
 `bot_id`:\<bot_id>                                |  string            | **Automatic** Value of the `--id` parameter specified in the bot configuration (Inserted by the framework)    |
 `frame_id`:\<frame_id>                            |  number            | **Automatic** Value of the `id` parameter specified in the call to `bot_message()` (Inserted by the framework |

For `rect`, the dimensions are fractions of a unit video frame with origin `0.0, 1.0` (top left), width `1.0`, and height `1.0`.

* To learn more about `bot_id`, see [Video bot command-line parameters](reference.md#video-bot-command-line-parameters).
* To learn more about `frame_id`, see [`frame_id`](reference.md#frame-id).
* To learn more about `bot_message()` see [Publish messages from a bot](tasks.md#publish-messages-from-a-bot)

### Counted objects

A `counted_objects` message contains information about objects in the specified frames that another program is
expected to count. In the Satori system, a counted object is something in the frame that the image processing algorithm
matched to an object that should be counted. A subscriber program receives counted object messages and decides what
to count.

The following JSON object is an example of a `counted_object` message you might publish with `bot_message`:
```json
{
    "counted_objects" : [
        {
            "rect": [0.229, 0.508, 0.200, 0.300],
            "id" : "car",
            "tag" : "vehicle",
            "color" : "yellow"
        },
        {
            "rect": [0.300, 0.400, 0.127, 0.150],
            "id" : "car",
            "tag" : "vehicle",
            "color" : "yellow"
        }
    ]
}
```

|     Entity                                       |   Value type       |  Description
| :----------------------------------------------- | :----------------: | :------------------------------------------------------------------------------------------------------------ |
 `"counted_objects"`                               |  string            | Handle this data as counted  objects                                                                          |
 (list of objects)                                 |  object\[\]        | A list of the objects counted                                                                                 |
 (object)                                          |  object            | Attributes of each counted object                                                                             |
 `"rect"`:`[x, y, width, height]`                  |  array of number   | Dimensions of the rectangle that encloses an object format                                                    |
 `id`:\<identifier>                                |  string            | **Optional** identifier that's specific to this object                                                        |
 `tag`:\<tag>                                      |  string            | **Optional** tag for this type of object                                                                      |
 `color`:\<color>                                  |  string            | **Optional** color to use for the enclosing rectangle when displayed                                          |
 `bot_id`:\<bot_id>                                |  string            | **Automatic** Value of the `--id` parameter specified in the bot configuration (Inserted by the framework)    |
 `frame_id`:\<frame_id>                            |  number            | **Automatic** Value of the `id` parameter specified in the call to `bot_message()` (Inserted by the framework |

For `rect`, the dimensions are fractions of a unit video frame with origin `0.0, 1.0` (top left), width `1.0`, and height `1.0`.

* To learn more about `bot_id`, see [Video bot command-line parameters](reference.md#video-bot-command-line-parameters).
* To learn more about `frame_id`, see [`frame_id`](reference.md#frame-id).
* To learn more about `bot_message()` see [Publish messages from a bot](tasks.md#publish-messages-from-a-bot)

### Label messages
A `labels` message contains information about the labels detected for the specified frames. Labels are text strings
assigned by the image processing software to entities it detects in the frames.

The following JSON object is an example of a `labels` message you might publish with `bot_message`:
```json
{
    "labels" : [
        { "text" : "horse"},
        { "text" : "submarine"}
    ]
}
```
|     Entity             |   Value type | Description                                                                                                      |
| :--------------------- | :----------: | :--------------------------------------------------------------------------------------------------------------- |
 `"labels"`              |  string      | The objects described in this message are labels detected in the frames                                          |
 (list of objects)       |  object\[\]  | A list of labels                                                                                                 |
 (object)                |  object      | A label                                                                                                          |
 `"text"`:\<string>      |  string      | The label text                                                                                                   |
 `bot_id`:\<bot_id>      |  string      | **Automatic** Value of the `--id` parameter specified in the bot configuration (Inserted by the framework).      |
 `frame_id`:\<frame_id>  |  number      | **Automatic** Value of the `id` parameter specified in the call to `bot_message()` (Inserted by the framework).  |

* To learn more about `bot_id`, see [Video bot command-line parameters](reference.md#video-bot-command-line-parameters).
* To learn more about `frame_id`, see [`frame_id`](reference.md#frame-id).
* To learn more about `bot_message()` see [Publish messages from a bot](tasks.md#publish-messages-from-a-bot)

### Debug message

A `message` message contains debug information about unusual conditions encountered by a bot.
The following JSON object is an example of a `labels` message you might publish with `bot_message`:
```json
{
    "message" : "Tracking failed",
    "rect": [0.225, 0.2167, 0.0625, 0.0584]
}
```
|     Entity                      |  Value type   | Description                                                                                                     |
| :------------------------------ | :-----------: | :-------------------------------------------------------------------------------------------------------------- |
 `"message"`                      |  string       | The objects in this message are debug messages                                                                  |
 (string)                         |  string       | A debug message                                                                                                 |
 `"rect"`:`[x, y, width, height]` |  string       | The rectangle in the frame that's associated with the problem.                                                  |
 `bot_id`:\<bot_id>               |  string       | **Automatic** Value of the `--id` parameter specified in the bot configuration (Inserted by the framework).     |
 `frame_id`:\<frame_id>           |  number       | **Automatic** Value of the `id` parameter specified in the call to `bot_message()` (Inserted by the framework). |

For `rect`, the dimensions are fractions of a unit video frame with origin `0.0, 1.0` (top left), width `1.0`, and height `1.0`.

* To learn more about `bot_id`, see [Video bot command-line parameters](reference.md#video-bot-command-line-parameters).
* To learn more about `frame_id`, see [`frame_id`](reference.md#frame-id).
* To learn more about `bot_message()` see [Publish messages from a bot](tasks.md#publish-messages-from-a-bot)

### Configuration message
The bot framework passes configuration settings from the command line to your configuration callback function. This
occurs only once, when your bot is initializing.

The following JSON object is an example of a configuration you might receive in the `message` parameter of your
configuration callback:
```json
{
    "action": "configure",
    "body": {
      "set_threshold" : 67
    }
}
```
This message provides a threshold value for converting images. After your configuration callback receives this
setting, put its values into your bot context so that your image processing callback has access to them.

