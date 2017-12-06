# Satori Video C++ SDK Examples

## Prerequisites
* Mac OS X or Linux
* C++14 compiler
* Other tools. See [Prerequisites](prerequisites.md)

## Clone the example code
Clone the example code from the
[GitHub examples project](https://github.com/satori-com/satori-video-sdk-cpp-examples):
```shell
$ git clone git@github.com:satori-com/satori-video-sdk-cpp-examples.git
$ cd satori-video-sdk-cpp-examples
```

## Build the video bot
**Note:** The initial build needs more time than subsequent builds,
because `conan` package manager needs to retrieve and build the necessary
packages.

**To build an empty bot with the `OpenCV` wrapper:**
```shell
$ cd empty-opencv-bot
$ mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release ../ && make -j8
```

**To build an an empty bot without `OpenCV`:**
```shell
$ cd empty-bot
$ mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release ../ && make -j8
```

**To test the empty bot:**
Run the empty bot without arguments. The bot displays a usage summary:
```
$ ./empty-bot
```

## Bot execution examples
**Run the video bot with a video file as input**
```shell
./empty-bot --input-video-file=my_video_file.mp4
```

**Run the video bot with a Satori video stream as input**
```shell
./empty-bot --endpoint=<satori-endpoint> --appkey=<satori-appkey> --channel=<satori-channel>
```

See the section [Running Video Bots](using_sdk.md#running-video-bots) section in the user guide to learn more about configuring
video bot inputs and run parameters.

## Configure the video bot
You can configure a video bot in the following ways:

* **Direct:** Use command line parameters
* **Configuration file:** Use parameters in a configuration file you specify on the command line.
Format the file as a JSON object. The contents appear as a message in the control channel for the bot, which passes the message to
your control channel callback function in CBOR format.
* **Control channel:** Publish messages to the control channel for the bot. You can publish the
data in JSON or CBOR format. The bot passes the message to your control
channel callback function in CBOR format.

To learn more about using CBOR in C++, see the documentation for [libcbor](http://libcbor.org/)).
and passed to bot's control callback (more information [here](using_sdk.md#video-bots-architecture-and-lifecycle)).
### Bot configuration examples
**Pass the configuration in command line arguments:**
```shell
$ ./haar-cascades-bot --input-video-file my_video_file.mp4 --config "{\"frontalface_default.xml:\": \"a face\", \"smile.xml\": \"a smile\"}"
```
**Pass the configuration as a file:**
```shell
$ ./haar-cascades-bot --input-video-file my_video_file.mp4 --config-file my_config.json
```

In both of these examples, the bot passes the following message to the control callback function:
```json
{
  "action": "configure",
  "body": {
    "frontalface_default.xml": "a face",
    "smile.xml": "a smile",
  }
}
```

**C++ code that configures a bot to use Haar cascades for object detection:**
```c++
struct cascade {
  cv::CascadeClassifier classifier;
  std::string tag;
};

struct state {
  std::list<cascade> cascades;
  uint32_t detection_id{0};
};

std::string cbor_to_string(const cbor_item_t *item) {
  assert(cbor_isa_string(item));

  return std::string{reinterpret_cast<char *>(cbor_string_handle(item)),
                     cbor_string_length(item)};
}

//
// Control channel callback function
// The bot invokes this callback at least once, during initialization
// If the --config-file parameter is present on the command line that
// invokes the bot, the bot passes the contents of the config file to
// the function in the config parameter.
//
cbor_item_t *process_command(sv::bot_context &context, cbor_item_t *config) {
  CHECK_S(cbor_isa_map(config));

  cbor_item_t *action{nullptr};
  cbor_item_t *body{nullptr};

  for (size_t i = 0; i < cbor_map_size(config); i++) {
    cbor_item_t *key = cbor_map_handle(config)[i].key;
    cbor_item_t *value = cbor_map_handle(config)[i].value;

    const std::string key_str = cbor_to_string(key);

    if (key_str == "action")
      action = value;
    else if (key_str == "body")
      body = value;
  }

  CHECK_NOTNULL_S(action);
  CHECK_NOTNULL_S(body);
  CHECK_S(cbor_isa_string(action));

  if (cbor_to_string(action) == "configure") {
    CHECK_S(context.instance_data == nullptr);

    const size_t body_size = cbor_map_size(body);
    CHECK_GT_S(body_size, 0) << "Configuration was not provided";

    std::unique_ptr<struct state> state = std::make_unique<struct state>();

    for (size_t i = 0; i < body_size; i++) {
      const cbor_item_t *key = cbor_map_handle(body)[i].key;
      const cbor_item_t *value = cbor_map_handle(body)[i].value;

      CHECK_S(cbor_isa_string(value));

      std::string cascade_file = cbor_to_string(key);
      std::string cascade_tag = cbor_to_string(value);

      struct cascade cascade;

      if (!cascade.classifier.load("models/" + cascade_file)) {
        ABORT_S() << "Can't load classifier " << cascade_file;
      }

      cascade.tag = cascade_tag;
      state->cascades.push_back(std::move(cascade));
    }

    context.instance_data = state.release();
    LOG_S(INFO) << "Bot is initialized";
  }

  return nullptr;
}
```

## Process image frames

The bot passes image frames to the image callback function you provide:
(more information [here](using_sdk.md#video-bots-architecture-and-lifecycle)).

**C++ code that uses Haar cascades to detect objects in images:**
```c
cbor_item_t *build_object(const cv::Rect &detection, uint32_t id,
                          const cv::Size &image_size, const std::string &tag) {
  cbor_item_t *object = cbor_new_definite_map(3);
  CHECK_NOTNULL_S(object);

  cbor_map_add(object,
               {cbor_move(cbor_build_string("id")), cbor_move(cbor_build_uint32(id))});
  cbor_map_add(object, {cbor_move(cbor_build_string("rect")),
                        cbor_move(sv::opencv::rect_to_cbor(
                            sv::opencv::convert_to_fractional(detection, image_size)))});
  cbor_map_add(object, {cbor_move(cbor_build_string("tag")),
                        cbor_move(cbor_build_string(tag.c_str()))});

  return object;
}

cbor_item_t *build_analysis_message(cbor_item_t *objects) {
  cbor_item_t *analysis_message = cbor_new_indefinite_map();
  CHECK_NOTNULL_S(analysis_message);

  cbor_map_add(analysis_message,
               {cbor_move(cbor_build_string("objects")), cbor_move(objects)});

  return analysis_message;
}

//
// image callback function
// The bot invokes this function every time it assembles a new frame
// from the input channel
//
void process_image(sv::bot_context &context, const cv::Mat &image) {
  auto state = static_cast<struct state *>(context.instance_data);

  const cv::Size image_size{image.cols, image.rows};

  cbor_item_t *objects = cbor_new_indefinite_array();
  CHECK_NOTNULL_S(objects);

  size_t objects_counter = 0;

  for (auto &cascade : state->cascades) {
    std::vector<cv::Rect> detections;
    cascade.classifier.detectMultiScale(image, detections);

    for (const auto &detection : detections) {
      cbor_array_set(objects, objects_counter++,
                     cbor_move(build_object(detection, state->detection_id++, image_size,
                                            cascade.tag)));
    }
  }

  if (objects_counter == 0) {
    cbor_decref(&objects);
    return;
  }

  //
  // Publishes analysis data to the analytics subchannel for the bot
  // Although you can call bot_message at any time, the bot aggregates
  // the messages and publishes them when you return from your current
  // scope to the main event loop
  //
  bot_message(context, sv::bot_message_kind::ANALYSIS,
              cbor_move(build_analysis_message(objects)));
}
```

## Publish Analysis Results

**To write out analysis data from the bot:**
```c
bot_message(context, sv::bot_message_kind::ANALYSIS, cbor_move(build_analysis_message(objects)));
```

Depending on your configuration, bot_message publishes the data to:
 * An associated Satori analysis channel
 * `stdout`
 * An output file

### Examples

**Publish to the analysis subchannel:**
```shell
$ ./my-bot --endpoint <satori-endpoint> --appkey <satori-appkey> --channel <satori-channel>
```

**Write results to `stdout`:**
```shell
$ ./my-bot --input-video-file my_video_file.mp4
```

**Write results to an output file:**
```
$ ./my-bot --input-video-file my_video_file.mp4 --analysis-file output_analysis_file.json
```

## Deploy the video bot

Use [docker](https://www.docker.com/) to package and video bots.

Pack the bot program into a docker container and specify it as an entry point.
You can see examples [here](https://github.com/satori-com/satori-video-sdk-cpp-examples)).

Use one container to build your bot executable, and another to run
the deployed bot. For example:

* [Dockerfile for *building* the `haar-cascades` example bot](https://github.com/satori-com/satori-video-sdk-cpp-examples/blob/master/haar-cascades-bot/Dockerfile),
* [Dockerfile for *running* the `haar-cascades` example bot](https://github.com/satori-com/satori-video-sdk-cpp-examples/blob/master/haar-cascades-bot/image/Dockerfile).

If no docker orchestration tool is used, then you can run a bot in `docker` like this:
```shell
$ docker run --rm -ti haar-cascades-bot --id sample-haar-cascades-bot --endpoint <satori-endpoint> --appkey <satori-appkey> --channel <satori-channel> --config "{\"frontalface_default.xml\": \"a face\", \"smile.xml\": \"a smile\"}"
```

If you use [Kubernetes](https://kubernetes.io/) you have several choices for deploying a container for your bot.
For example, your replication controller manifest could look like this:
```yaml
apiVersion: v1
kind: ReplicationController
metadata:
  name: face-detection
spec:
  replicas: 1
  template:
    metadata:
      labels:
        app: face-detection
    spec:
      hostNetwork: true
      containers:
      - name: face-detection
        image: gcr.io/video/haar-cascades-bot:latest
        args: [
          "--id", "my-face-detection-bot",
          "--endpoint", <satori-endpoint>,
          "--appkey", <satori-appkey>,
          "--channel", <satori-channel>,
          "--input-resolution", "640x480",
          "--config", "{\"frontalface_default.xml\": \"face\"}"
          ]
        resources:
          requests:
            memory: 256Mi
            cpu: 4
          limits:
            memory: 256Mi
            cpu: 4
```
