# Satori Video C++ SDK Tutorial

Supported platforms: Mac & Linux with C++14 compiler. [Install Prerequisites](prerequisites.md)

## Building First Video Bot

Clone [examples project](https://github.com/satori-com/satori-video-sdk-cpp-examples):

```shell
git clone git@github.com:satori-com/satori-video-sdk-cpp.git
cd satori-video-sdk-cpp
```

Build an empty-opencv-bot (or empty-bot if you prefer to work without `OpenCV` wrapper):

```shell
cd empty-opencv-bot
mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release ../ && make -j8
```

First time build will take significant time while `conan` fetches and builds required packages.


## Running Video Bot

Run bot without arguments to see list of available inputs.

Examples:

Run video bot with video file:

```shell
./empty-bot --input-video-file=my_video_file.mp4
```

Run video bot with Satori video stream:
```shell
./empty-bot --endpoint=<satori-endpoint> --appkey=<satori-appkey> --channel=<satori-channel>
```

Check out [Running Video Bots](using_sdk.md#running-video-bots) section in user guide for more ways to configure
bot input or tweak its running parameters.

## Configuring Bot

Configuration can be passed to a bot via command line argument or configuration file.
Configuration is expected to be in `JSON` format, it will be then transformed into `CBOR` item (read about [libcbor](http://libcbor.org/))
and passed to bot's control callback (more information [here](using_sdk.md#video-bots-architecture-and-lifecycle)).

Examples:
```shell
# Passing configuration as a command line argument
./haar-cascades-bot --input-video-file my_video_file.mp4 --config "{\"frontalface_default.xml:\": \"a face\", \"smile.xml\": \"a smile\"}"

# Passing configuration as a file
./haar-cascades-bot --input-video-file my_video_file.mp4 --config-file my_config.json
```

Bot's control callback will get the following object:
```json
{
  "action": "configure",
  "body": {
    "frontalface_default.xml": "a face",
    "smile.xml": "a smile",
  }
}
```

This example shows how to configure a bot that uses Haar cascades for object detection:
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

## Processing Frames

Frames processing will be done by bot's image callback function
(more information [here](using_sdk.md#video-bots-architecture-and-lifecycle)).

This example shows how a bot uses Haar cascades to detect objects in images:
```c++
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

  bot_message(context, sv::bot_message_kind::ANALYSIS,
              cbor_move(build_analysis_message(objects)));
}
```

## Sending Analysis Results
After processing is complete, bot sends an analysis message to analysis downstream by calling this function:
```c++
bot_message(context, sv::bot_message_kind::ANALYSIS, cbor_move(build_analysis_message(objects)));
```

Analysis downstream could be one of the following (depending on bot configuration.):
 * an associated Satori analysis channel,
 * stdout,
 * or, an output file.

```shell
# If a bot processes Satori video stream, then analysis results will be sent to an associated Satori analysis channel
./my-bot --endpoint <satori-endpoint> --appkey <satori-appkey> --channel <satori-channel>

# Using stdout as an analysis downstream
./my-bot --input-video-file my_video_file.mp4

# Using an output file as an analysis downstream
./my-bot --input-video-file my_video_file.mp4 --analysis-file output_analysis_file.json
```

## Deploy Video Bot
It is recommended to use [docker](https://www.docker.com/) for packaging video processing bots.
Bot's executable needs to be packed into a container and specified as an entry point
(examples can be found [here](https://github.com/satori-com/satori-video-sdk-cpp-examples)).
It is a good practice to separate building bot executables from deployment images,
so, one docker container is responsible for building bot executable like
[here](https://github.com/satori-com/satori-video-sdk-cpp-examples/blob/master/haar-cascades-bot/Dockerfile),
and, another docker container contains only bot executable and environment necessary for it's execution like
[here](https://github.com/satori-com/satori-video-sdk-cpp-examples/blob/master/haar-cascades-bot/image/Dockerfile).

If no docker orchestration tool is used, then a bot could be run like this:
```shell
docker run --rm -ti haar-cascades-bot --id sample-haar-cascades-bot --endpoint <satori-endpoint> --appkey <satori-appkey> --channel <satori-channel> --config "{\"frontalface_default.xml\": \"a face\", \"smile.xml\": \"a smile\"}"
```

In case of [Kubernetes](https://kubernetes.io/) there are multiple ways of deploying containers.
For example, replication controller manifest could look like the following:
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
