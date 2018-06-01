# Satori Video SDK for C++: Tutorial

[All Video SDK documentation](https://github.com/satori-com/satori-video-sdk-cpp/blob/master/README.md)

## Overview
This tutorial shows you how to develop a Satori Video SDK bot that uses the
[OpenCV](https://www.opencv.org) C++ library to analyze a video stream. The video
bot publishes its results to a Satori channel, which another bot can subscribe to in order to do more processing.

The tutorial is based on the `motion-detector-bot` example bot. The source code for this bot is part of the
[Satori Video SDK for C++ Examples](https://github.com/satori-com/satori-video-sdk-cpp-examples)
repository.

**Note:** This tutorial shows you how to process video using the Satori video bot architecture. Although the architecture
can statically link the open source OpenCV2 image processing library into a video bot program, the tutorial doesn't
describe how to use the library.

The tutorial describes these steps:

1. Record a test video file
2. Set up prerequisites
3. Create the bot code
4. Publish the video file to the input channel
5. Test the bot against the input channel
6. Publish configuration messages to the control channel
7. Test the configuration messages

## Conventions
To make the steps easier to follow, the tutorial uses the following conventions:
* The tutorial describes a video bot that analyzes video published from a video file, but your production bots can
also publish video from a webcam.
* The name of the file that `satori_video_recorder` records to is `camera_output_file.mp4`. In your production bots,
you can use any name you want.
* The name of the channel that `satori_video_publisher` uses to publish streaming video is `videostream`. In your
production bots, you can use any name you want.

## Tutorial steps

### Set up prerequisites
Install the prerequisites described in [Satori Video SDK for C++ Prerequisites](prerequisites.md). Be sure to create
a Satori project for the tutorial, and record the appkey and endpoint.

### Create the bot program
The bot uses OpenCV-oriented API calls to set up and start the main processing loop. When the loop starts, it does
the following:
* Receives streaming video messages from the input channel
* Converts and decompresses the video into individual frames
* Invokes the image processing callback function you provide, passing in a single frame
* Publishes messages when you call `bot_message()`
* Whenever a new message appears in the control channel, invokes the command processing callback function you provide

#### Set up the source directory
Although the SDK examples repository contains a version of the tutorial code, you should try to follow the tutorial by
writing the C++ code from scratch. Copy the `empty-bot` example from the repository and modify it:

1. In a browser, navigate to `https://www.github.com/satori-com/satori-video-sdk-cpp-examples`
2. Click **Clone or download**, then click **Download ZIP**.<br>
3. Uncompress the zip file and copy the `empty-bot` directory to a new `motion-detector-bot` directory:<br>
`$ cp -r empty-bot <path>/motion-detector-bot`
4. Navigate to `<path>/motion-detector-bot`.
5. Delete `src/main.cpp` and `README.md`.
6. Edit `<path>/motion-detector-bot/CMakelists.txt`:
    * Change all occurrences of `empty-bot` to `motion-detector-bot`.
    * Change all occurrences of `src/main.cpp` to `src/motion-detector-bot.cpp`.

7. Edit `<path>/motion-detector-bot/conanfile.txt`:

    * Find the line that contains `SatoriVideo:with_opencv=False`. Change `False` to `True`.
    * Find the line that contains `[generators]`.
    * The next line is `cmake`; after this line, add a new line containing `virtualenv`.

7. Edit `<path>/motion-detector-bot/Dockerfile` and change all occurrences of `empty-bot` to `motion-detector-bot`.
8. Edit `<path>/motion-detector-bot/Makefile` and change all occurrences of `empty-bot` to `motion-detector-bot`.
Ignore all the other file references.

#### Add the primary functions
To define the overall bot structure and call the functions that start the video bot main processing loop,
add the following code to `motion_detector_bot/src/motion_detector_bot.cpp`:

```cpp
namespace sv = satori::video;
namespace motion_detector_bot {
    /*
    * Block A: In subsequent steps, you add structs and functions to this block
    */
    namespace {

        /*
        * Block B: In subsequent steps, you add variables and functions to this block
        */

    } // end namespace

    /*
    * Location for Block A structs
    */

    /*
    * Location for Block A functions
    */

} // end motion_detector_bot namespace

/*
* Motion detector bot program
*/

int main(int argc, char *argv[]) {
  // Disables OpenCV thread optimization
  cv::setNumThreads(0);
  // Registers the image processing and configuration callbacks
  sv::opencv_bot_register(
      {&motion_detector_bot::process_image, &motion_detector_bot::process_command});
  // Starts the main processing loop
  return sv::opencv_bot_main(argc, argv);
}
```

**Discussion**
* `opencv_bot_register()` is the OpenCV-enabled version of the video API `bot_register()` function. It
updates global areas in the SDK with the names of your image processing and configuration processing callbacks. It also
sets up other parts of the video bot, including OpenCV.
* `opencv_bot_main()` is the OpenCV-enabled version of the video SDK API `bot_main()` function. It starts the SDK's main
processing loop, which decompresses and decodes streaming video in the input channel and passes individual frames to
`process_image()`. The main processing loop also invokes `process_command()` when a message arrives from the control
channel.

#### Add headers
At the beginning of `motion_detector_bot.cpp`, add the following:

```cpp
#include <prometheus/counter.h>
#include <prometheus/counter_builder.h>
#include <prometheus/family.h>
#include <prometheus/histogram.h>
#include <prometheus/histogram_builder.h>
#include <satorivideo/opencv/opencv_bot.h>
#include <satorivideo/opencv/opencv_utils.h>
#include <satorivideo/video_bot.h>
#include <cstdlib>
// Boost timer library
#include <boost/timer/timer.hpp>
// GNU scientific library
#include <gsl/gsl>
// JSON for Modern C++
#include <json.hpp>
#include <opencv2/opencv.hpp>

#define LOGURU_WITH_STREAMS 1
// Loguru for Modern C++
#include <loguru/loguru.hpp>
```

**Discussion**
These statements include headers for the Prometheus metrics platform, the Satori video SDK, Boost, the GNU scientific
library (`gsl`), the Modern JSON for C++ library, the OpenCV2 library, and the Loguru logging library. The
`#define` statement causes Loguru to use C++ streams for logging.

The video bot build process installs the libraries themselves.

#### Create the image processing function
Replace the following comment:
```c++
    /*
    * Location for Block A functions
    */
```

with the code for the image processing callback, which in this tutorial is called `process_image`:

```c++
    /*
    * Invoked each time the SDK decodes a frame. The SDK passes in the bot context and an OpenCV Mat object.
    */
    void process_image(sv::bot_context &context, const cv::Mat &original_image) {
      /*
      * Points to the instance data area of the bot context. The default value is nullptr.
      */
      auto *s = (state *)context.instance_data;
      s->frames_counter.Increment();

      /*
      * Sets or declares control variables used by the OpenCV contour detection algorithm
      */
      cv::Size original_image_size{original_image.cols, original_image.rows};
      cv::Mat gaussian_blurred_image;
      cv::Mat morphed_image;
      /*
      * Point 1: See the next tutorial step
      */
    } // end process_image
```

**Discussion**
The image processing callback is the place to call APIs that analyze individual video frames. The tutorial uses the
OpenCV library, but you're free to use any libraries you want.

#### Process the image frame
To analyze the image frame, add calls to OpenCV APIs.

The `latency_reporter()` function measures the duration of an OpenCV operation and stores the result in the Prometheus
metrics registry, which is a member in the bot context. To conserve memory, each OpenCV call has its own block to
ensure that variables are destructed as soon as the operation finishes.

Replace the comment:
```cpp
      /*
      * Point 1: See the next tutorial step
      */
```

with the following code, which completes the definition of `process_image()`:

```cpp
      {
        latency_reporter reporter(s->blur_time);
        cv::GaussianBlur(original_image, gaussian_blurred_image, cv::Size(5, 5), 0);
      }
      {
        latency_reporter reporter(s->extract_time);
        s->background_subtractor->apply(gaussian_blurred_image, gaussian_blurred_image);
      }
      /*
      * Note: getStructuredElement() uses the feature_size_value variable stored in the instance_data member of
      * the bot context. You can change this variable dynamically by publishing a new value to the control channel.
      * To learn more, see the code for process_command())
      */
      {
        latency_reporter reporter(s->morph_time);
        cv::Mat element = cv::getStructuringElement(
            cv::MORPH_RECT, cv::Size(s->params.feature_size_value, s->params.feature_size_value));
        cv::morphologyEx(gaussian_blurred_image, morphed_image, cv::MORPH_OPEN, element);
      }
      std::vector<std::vector<cv::Point>> contours;
      std::vector<cv::Vec4i> contours_topology_hierarchy;
      {
        latency_reporter reporter(s->contours_time);
        cv::findContours(morphed_image, contours, contours_topology_hierarchy, CV_RETR_EXTERNAL,
                         CV_CHAIN_APPROX_SIMPLE);
      }
      if (contours.empty()) {
        return;
      }

      s->contours_counter.Increment(contours.size());
      /*
      * Publishes the results to the analysis channel.
      */
      publish_contours_analysis(context, original_image_size, contours);
```

**Discussion**
Notice that the image processing function calls OpenCV to analyze frames, but it doesn't do full motion
detection. Instead, it publishes results to the analysis channel. Another bot can subscribe to this channel and
do further processing to detect motion.

Splitting the work makes the overall system more flexible and prevents latency bottlenecks. The video
bot publishes first-step results in a compact format. To persist analysis messages in the channel, you can adjust
channel settings in Dev Portal. Another bot then has time to do further analysis.

Using a second bot also lets you store results for several frames outside the video bot. This lets the video bot
use memory and bandwith more efficiently.

#### Add helper definitions
Now add the helper definitions that `process_image()` uses. These are:
* `state`: A struct that defines names in the bot context instance data area
* `latency_reporter()`: Tracks time delays for OpenCV API calls
* `publish_contours_analysis()`: Publishes contour data to the analysis channel.

##### Add `state`
Replace the following comment (immediately before the declaration of `process_image()`):
```cpp
    /*
    * Location for Block A structs
    */
```

with the following code:

```cpp
    /*
    * Sets up storage in the bot context as members of the struct
    * The video SDK API defines the members "metrics" and "registry"; see video_bot.h
    *
    * This struct defines counters, buckets, and timers for Prometheus, sets up a member that
    * stores the featureSize configuration (params), and adds a BackgroundSubtractor instance.
    *
    */
    struct state {
      /*
      * Constructor
      */
      explicit state(sv::bot_context &context)
        /*
        * Initializes Prometheus counters and histograms in the bot context metrics registry
        */
        :
        frames_counter(prometheus::BuildCounter()
            .Name("frames")
            .Register(context.metrics.registry)
            .Add({})),
        contours_counter(prometheus::BuildCounter()
            .Name("contours")
            .Register(context.metrics.registry)
            .Add({})),
        blur_time(prometheus::BuildHistogram()
            .Name("motion_detector_blur_times_millis")
            .Register(context.metrics.registry)
            .Add({}, std::vector<double>(latency_buckets))),
        extract_time(prometheus::BuildHistogram()
            .Name("motion_detector_extract_times_millis")
            .Register(context.metrics.registry)
            .Add({}, std::vector<double>(latency_buckets))),
        morph_time(prometheus::BuildHistogram()
            .Name("motion_detector_morph_times_millis")
            .Register(context.metrics.registry)
            .Add({}, std::vector<double>(latency_buckets))),
        contours_time(prometheus::BuildHistogram()
            .Name("motion_detector_contours_times_millis")
            .Register(context.metrics.registry)
            .Add({}, std::vector<double>(latency_buckets)))
      {}

      /*
      * Stores parameters received in process_command
      */
      parameters params;

      cv::Ptr<cv::BackgroundSubtractorKNN> background_subtractor{
          cv::createBackgroundSubtractorKNN(500, 500.0, true)
      };

      /*
      * Intermediate members that store Prometheus metrics
      */
      prometheus::Counter &frames_counter;
      prometheus::Counter &contours_counter;
      prometheus::Histogram &blur_time;
      prometheus::Histogram &extract_time;
      prometheus::Histogram &morph_time;
      prometheus::Histogram &contours_time;
    };
```

##### Add `latency_buckets`
Define a constant that initializes the buckets used by the Prometheus histograms. These metrics provide data about
the latency of OpenCV API calls. For each histogram, each bucket is a duration range in seconds, and the values
define the beginning of the range. The end of the range defaults to the beginning of the next range.

Replace the comment:
```cpp
        /*
        * Block B: In subsequent steps, you add variables and functions to this block
        */
```

with the following code:

```cpp
    /*
    * Defines a constant for initializing the latency buckets for a Prometheus histogram. For example, the first
    * bucket is 0 to 0.1 seconds, the second bucket is .1 to .2 seconds, and so forth. The last bucket is 900 seconds
    * and above.
    */
    constexpr std::initializer_list<double> latency_buckets = {
        0,  0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1,  2,  3,
        4,  5,   6,   7,   8,   9,   10,  20,  30,  40,  50, 60, 70,
        80, 90,  100, 200, 300, 400, 500, 600, 700, 800, 900};
```

**Discussion**
The latency metrics that Prometheus collects for each OpenCV operation help you detect bottlenecks, take remedial
action, and observe the effect.

##### Add `latency_reporter`
Define a function that starts timing the duration of an OpenCV API call.

Add the following code just after the definition of `latency_buckets`:

```cpp
    /*
    * Uses Prometheus to time the duration of an image processing operation.
    * To reduce memory usage, latency_reporter() does its work during its destructor.
    */
    class latency_reporter {
    // Constructor. Initializes the histogram object
      public:
        explicit latency_reporter(prometheus::Histogram &hist) : _hist(hist) {}
        /*
        * Destructor. Just before cleanup, records the elapsed time since initialization.
        * Timer returns milliseconds, so divide by 1 million to get seconds
        */
        ~latency_reporter() { _hist.Observe(_timer.elapsed().wall / 1e6); }

       private:
        prometheus::Histogram &_hist;
        boost::timer::cpu_timer _timer;
    };
```

##### Add `publish_contours_analysis`
Define a function that publishes the results of `process_image()` to the analysis channel.

The function adds meta-data to the results before publishing.

To do the actual publishing, `publish_contours_analysis` calls the video bot API function `bot_message()`.

Add the following code just before the definition of `latency_buckets`:

```cpp
    /*
    * Publishes the results of analyzing a frame to the analysis channel
    * Adds meta-data fields to each message
    */
    void publish_contours_analysis(sv::bot_context &context, const cv::Size &original_size,
                                const std::vector<std::vector<cv::Point>> &contours) {
      // Instantiates a JSON array
      nlohmann::json rects = nlohmann::json::array();
      // Iterates over the input vector
      for (const auto &contour : contours) {
        auto rect = boundingRect(contour);
        // Instantiates a JSON object to hold the input vector
        nlohmann::json obj = nlohmann::json::object();
        // Sets the JSON object meta-data
        obj["id"] = 1;
        obj["color"] = "green";
        // Scales the input vector
        obj["rect"] = sv::opencv::to_json(sv::opencv::to_fractional(rect, original_size));
        // Adds the resulting JSON object to an array of objects
        rects.emplace_back(std::move(obj));
      }
      // Instantiates a JSON object for the message
      nlohmann::json analysis_message = nlohmann::json::object();
      // Sets the key for the message to "detected_objects" and the value to the array of objects
      analysis_message["detected_objects"] = rects;
      /*
      * Publishes the message to the analysis channel
      */
      sv::bot_message(context, sv::bot_message_kind::ANALYSIS, std::move(analysis_message));
    }
```

**Discussion**
`bot_message()` takes up to four parameters:
* bot context
* `bot_message_kind`: Determines which channel the SDK uses to publish the message. In this case, the code calls
`bot_message()` with the `enum` constant for the analysis channel, `bot_message_kind::ANALYSIS`.
##### Add `parameters`
The `parameters` struct defines functions that copy a new feature size value from an incoming control channel message to
the `instance_data` member of the bot context.

Add the following code immediately before the code for `struct state`:
```cpp
    /*
    *  Moves configuration parameters from a configuration message to the bot context.
    *
    *  merge_json() sets parameters.feature_size_value to the value of the featureSize property in the message.
    *  to_json() returns a JSON property with key "featureSize" and value parameters.feature_size_value.
    */

    struct parameters {
      uint32_t feature_size_value{5};
      /*
      * Copies the feature size from the configuration message to feature_size_value.
      */
      void merge_json(const nlohmann::json &params) {
        if (!params.is_object()) {
          LOG_S(ERROR) << "ignoring bad params: " << params;
          return;
        }
        /*
        * Copies the feature size from the message to the local variable.
        */
        if (params.find("featureSize") != params.end()) {
          auto &featureSize_value = params["featureSize"];
          if (featureSize_value.is_number_integer()) {
            this->feature_size_value = featureSize_value;
          }
        }
      }
      /*
      * Returns the field "featureSize" with value set to the new feature size.
      * The SDK publishes this message to the control channel.
      */
      nlohmann::json to_json() const { return {{"featureSize", feature_size_value}}; }
    };
```

#### Create the command processing function
To receive and process image processing parameters, create the command processing function. The SDK
invokes this function in two situations:
* **Initialization:** Before it starts processing video, the SDK invokes the function and passes in any configuration
parameters you specify on the bot command line. To learn more about this command line option, see
the section [Video bot command-line syntax](reference.md#video-bot-command-line-syntax) in the topic
[Satori Video SDK for C++ Reference](reference.md).
* **Control channel message:** When the SDK receives a message in the control channel, it invokes the function and
passes in the message. This feature lets you dynamically update your bot while it's running.

The SDK invokes the command processing function with two parameters:
* `context`: The global bot context. To learn more about this struct, see [`bot_context`](reference.md#bot_context).
* `message`: A JSON object containing the received message

In this tutorial, the command processing function has the name `process_command()`. It sets the feature size passed to
the OpenCV API function `getStructuringElement()`. The size sets how big an image feature has to be in order to qualify
as an object of interest.

The function receives feature size from the command line `--config` parameter during initialization, and it also
receives feature size from the control channel.

Add the following code immediately after the definition of `process_image()`:

```cpp
    /*
    * Receives configurations and stores them in the bot context.
    *
    * The tutorial bot handles two message formats:
    *
    * 1. Initialization configuration
    * {
    *   "action": "configure", "body": { "featureSize": &lt;size&gt; }
    *
    * and
    *
    * 2. Updated configuration
    * {
    *   "to": "motion_tutorial", params": { "featureSize": &lt;size&gt; }
    * }
    */
    nlohmann::json process_command(sv::bot_context &context, const nlohmann::json &command_message) {
      /*
      * The bot context has pre-defined members, such as the metrics registry. Use the instance_data member to store
      * your own data. The tutorial uses instance_data to store members of the `state` struct.
      */
      auto *s = (state *)context.instance_data;
      /*
      * The received message must always be a JSON object, even during initialization
      */
      if (!command_message.is_object()) {
          LOG_S(ERROR) << "Control message is not a JSON object: " << command_message;
          return nullptr;
      }
      /*
      * The SDK invokes process_command() during initialization, before it starts ingesting video. Use this first invocation
      * to initialize instance_data.
      */
      if (s == nullptr) {
          context.instance_data = s = new state(context);
          LOG_S(INFO) << "Bot configuration initialized";
          // No other processing is needed.
          return nullptr;
      }
      /*
      * Handle initial configuration parameters passed on the command line
      *
      * The command line parameter is --config '{"featureSize": 5.0}'.
      * The SDK passes the JSON object to process_command in the message
      * {"action": "configure", "body": {"featureSize": 5.0}"
      *
      */
      /*
      * Tests if the incoming message is an initial configuration
      */
      if (command_message.find("action") != command_message.end()) {
          if (command_message["action"] != "configure") {
              LOG_S(ERROR) << "Invalid value for \"action\" key" << command_message;
              return nullptr;
          } else {
              // Ensures that an "action" message contains a "body" field
              if (command_message.find("body") != command_message.end()) {
                  auto &body_params = command_message["body"];
                  LOG_S(INFO) << "Received config parameters in \"action\": \"configure\": " << command_message;
                  // Doesn't move an empty object to the bot context
                  if (body_params != nullptr)
                  {
                      s->params.merge_json(body_params);
                  }
                  // Because the message doesn't contain a bot id, the function can't return an ack
                  return nullptr;
              // end of "body" field processing
              } else {
                  LOG_S(ERROR) << "\"action\": \"configure\" message doesn't contain \"body\"" << command_message;
                  return nullptr;
              }
          } // end of "configure" value processing
      } // end of "action" field processing
      /*
      * The message didn't contain the "action" field. Test to see if it's an "ack" message.
      */
      /*
      * This implementation of process_command() returns an "ack" message if it successfully processes a command
      * message. This message includes the field "ack": true (The SDK doesn't do this automatically; you have to
      * set up the message yourself).
      *
      * The SDK publishes the message back to the control channel. Because the SDK is also subscribed to the control
      * the ack message arrives back in process_command(). In general, you can do whatever you want with the ack
      * message. To avoid an endless loop, detect ack messages and ignore them. You can subscribe to the control
      * channel from another bot and process ack messages separately.
      * In this tutorial, process_command logs receipt of the ack and returns null to the SDK.
      */
      if (command_message.find("ack") != command_message.end()) {
          // This message is fully handled, so process_command() returns null to the SDK
          return nullptr;
      }
      /*
      * The command message doesn't contain the "ack" field, so it must contain new configuration parameters.
      * This block sets the parameters in the bot context to the values in the command message.
      */
      if (command_message.find("params") != command_message.end()) {
        auto &params = command_message["params"];
        LOG_S(INFO) << "Received config parameters: " << command_message;
        // Move the parameters to the context
        s->params.merge_json(params);
        /*
        * Gets the bot id from the command message. The "to" field is guaranteed to be present, because the SDK
        * returns an error and skips process_command() if the message doesn't contain the field
        */
        std::string bot_id = command_message["to"];
        /*
        * Returns the ack message to the SDK, which publishes it back to the control channel.
        * Sets up a JSON object that contains the following:
        * {"ack": true, "to": "motion_tutorial", "params": "featureSize": 5.0}
        * - "params" field contains the parameters in the original command message.
        * - "to" field contains the bot id specified on the command line.
        */
        // Initializes the return object by setting it to the "ack" field
        nlohmann::json return_object = {{"ack", true}};
        // Inserts the bot id field
        nlohmann::json to_object = {{"to", bot_id}};
        return_object.insert(to_object.begin(), to_object.end());
        // Inserts the configuration parameters from the incoming message
        nlohmann::json config_object = s->params.to_json();
        return_object.insert(config_object.begin(), config_object.end());
        LOG_S(INFO) << "Return ack message: " << return_object.dump();
        // Returns the ack
        return return_object;
      }
      // Control reaches here if the "params" key isn't found
      LOG_S(ERROR) << "Control message doesn't contain params key " << command_message;

      return nullptr;
    }
```

**Discussion**
With this function in place, you can change the feature size parameter, even when the bot is running. The new feature
size takes effect the next time `process_image()` retrieves feature size from the bot context. The function also
publishes an acknowledgement, which lets you confirm that the bot processed the message.

### Build the video bot
1. Navigate to your `motion-detector-bot` directory.
2. Create a `build` directory:<br>
`$ mkdir build && cd build`
3. Run `cmake` to set up the build files:<br>
`$ cmake -DCMAKE_BUILD_TYPE=Release ../`
4. Run `make` to build the bot. The `-j 8` cores parameter is optional; you can use as many cores as you want.

The build process does the following:

1. Runs `conan` to install dependencies
2. Sets up the make file for GNU make
3. If necessary, compiles dependencies
4. Compiles `motion-detector-bot.cpp`
5. Links the object file for `motion-detector-bot` with the object files for includes and dependencies
6. Creates a directory `motion-detector-bot/build/bin` containing the bot executable `motion-detector-bot`.

The process also creates shell scripts that control a virtual environment for the video SDK tools:
* `motion-detector-bot/build/activate.sh` turns on the virtual environment. It modifies your `$PATH` variable to
point to the conan local cache directory that contains the tools, and prepends the string `(conanenv)` to your command
prompt to remind you that the virtual environment is on.
* `motion-detector-bot/build/deactivate.sh` reverts your environment to its previous settings.

### Record a test video
Satori Video SDK bots process streaming video, so to test your bot you need to generate something for your bot to work
on.

Although you have several options for recording a video stream, the best one when you're starting out is the
`satori_video_recorder`, which records video input to a file. If you have a Macbook Pro, you can record video from its
built-in camera. For Linux, you have to record video from a webcam.

#### macOS

1. Turn on the virtual environment for the SDK tools:<br>
`$ source motion-detector-bot/build/activate.sh`
2. Record video from the laptop camera:
```terminal
(conanenv) $ satori_video_recorder --time-limit 30 --input-camera --output-video-file camera_output_file.mp4
```

3. The program displays a small window containing the video camera output. Using the window as a guide, move around
to provide moving object contours in the video stream. After 30 seconds, the program stops.

4. To test the file, enter the following in a terminal window:

```terminal
$ satori_video_player --time-limit 30 --input-video-file camera_output_file.mp4
```
You see the video play in a small window for 30 seconds, after which the video stops. Close the window to exit the
program.

Leave the virtual environment on as you do the next steps.

### Test the video bot
Test the video bot:

1. [Smoke test](#smoke-test)
2. [Video test](#video-test)
3. [Config test](#config-test)

#### Smoke test

1. Navigate to your `motion-detector-bot/build/bin` directory.
2. Run the bot:<br>
`(conanenv) $ ./motion-detector-bot`
3. The bot displays the usage message for video bots, including all of the possible parameters.

#### video test
To do a functional test of the video processing part of the bot:

1. Publish streaming video to the input channel.
2. Run the bot.
3. Look at the results in Dev Portal.

**Publish the video file to the input channel**
To publish the video file:

1. Check that you have the appkey and endpoint you obtained from Dev Portal in the step
[Set up prerequisites](#set-up-prerequisites).
2. Copy the video file `camera_output_file.mp4` to `motion-detector-bot/build`.
3. In a terminal window, navigate to `motion-detector-bot/build`.
4. To set up your environment for the video bot tools, enter `$ source ./activate.sh`. Your command prompt is now
prefixed with the string `(conanenv)`.
5. Using the `--loop` parameter to repeat the video, publish the video to the input channel:
```bash
(conanenv) $ satori_video_publisher --input-video-file camera_output_file.mp4 --loop --endpoint <your_endpoint> --appkey <your_appkey> --output-channel videostream
```

**Run the video bot**
In another terminal window, start the video bot:

```bash
$ bin/motion-detector-bot --endpoint <your_endpoint> --config '{"featureSize": 5.0}' --appkey <your_appkey> --input-channel videostream --id motion_tutorial
```
**Subscribe to the analysis channel in Dev Portal**

1. Navigate to the project you created in Dev Portal, then click the **Console** tab.
2. In the text field that contains the hint **Enter channel**, enter `videostream/analysis`, then click outside the
field. You should see messages scrolling through the window that appears after the **Code** field.
3. Click on a message to expand it. Its contents look similar to the following JSON:

```json
{
  "detected_objects": [
    {
      "color": "green",
      "id": 1,
      "rect": [
        0.496875,
        0.9111111111111111,
        0.03125,
        0.07777777777777778
      ]
    },
    {
      "color": "green",
      "id": 1,
      "rect": [
        0.603125,
        0.7555555555555555,
        0.01875,
        0.03333333333333333
      ]
    },
  ],
  "from": "motion_tutorial",
  "i": [
    19486316,
    19565723
  ]
}
```

Leave the bot running so you can test the control function.

**Discussion**

* The "detected_objects" array may contain more elements, depending on the contents of each image frame.
* The "from" field contains the value you specified for the `--id` parameter when you ran the bot.
* The "i" field contains the beginning and ending frame for the results in this message.

#### Config test
To test the configuration part of the video bot:

1. In Dev Portal, subscribe to the control channel.
2. In Dev Portal, publish configuration messages to the control channel
3. Review the log messages from the bot and the messages published to the control channel from the bot.

**Subscribe to the control channel in Dev Portal**

1. Navigate to the project you created in Dev Portal. If you're on the **Console** tab, navigate away from it.
2. Click the **Console** tab.
3. In the text field that contains the hint **Enter channel**, enter `videostream/control`, then click outside the
field. The message **Successfully subscribed** appears after the **Code** field. Nothing appears, because you're not yet
publishing messages to the channel.

Leave the console displayed.

**Publish configuration messages to the control channel**

1. In the lower right-hand corner of the console, click the orange text icon to display the **Publish Message** panel.
2. Enter the following JSON on the first line:<br>
`{ "to": "motion_tutorial", "params": {"featureSize": 5.0}}`<br>
3. On the next line, labeled **Count**, enter 100.
4. Click **Send**

This publishes the test control message 100 times, which helps you find and review the test results.

**Examine the results**
Navigate back to Dev Portal. On the **Console** tab, you should still be subscribed to `videostream/control`. You
should see messages in the window that appears after the **Code** field. Click on a message to expand it.
Its contents look similar to the following JSON:
```
{
  "ack": true,
  "featureSize": 5,
  "from": "motion_tutorial",
  "i": [
    0,
    0
  ],
  "to": "motion_tutorial"
}
```

This message confirms that the command processing callback function received the configuration update you published.

Return to the terminal window in which your video bot is running. The bot logs several messages as it starts up.

**Look for initial configuration**
Just after the command line that you used to run the bot, look for a log message similar to<br>
```
Starting secure RTM client: <endpoint>, appkey: <appkey>
```

`<endpoint>` and `<appkey>` are the values for your project.

After that line, you should lines similar to the following:
```
[main thread     ]       bot_instance.cpp:278      0| configuring bot: {"action":"configure","body":{"featureSize":5.0}}
[main thread     ]motion_detector_bot.cpp:265      0| Bot configuration initialized
```

These log messages show you that the bot received the configuration you specified on the command line with the `--config`
parameter (see the previous step, **Run the video bot**).

**Look for configuration updates**

Next, look further on in the log messages for a message similar to<br>
```
[decoder_videost ]      threaded_worker.h:94       0| 0x1051cd2c0 decoder_videostream started worker thread`
```

Following this message, the bot logs messages from your control callback function, similar to:
```
motion_detector_main.cpp:288      0| process_command: Received config parameters: {"params":{"featureSize":5.0},"to":"motion_tutorial"}
motion_detector_main.cpp:314      0| Return ack message: {"ack":true,"featureSize":5,"to":"motion_tutorial"}
```

The first line shows that `process_command()` received a configuration message from the control channel. The
second line shows that `process_command()` successfully processed the message and published an acknowledgement back to
the control channel.

**Stop `satori_video_publisher` and the tutorial bot**

1. In the terminal window that's running `satori_video_publisher`, enter `<CTRL>+C` to terminate the program.
2. In the terminal window that's running the video bot, enter `<CTRL>+C` to terminate the bot.


**Discussion**
* When you deploy a video bot to production, you should reduce the number of `LOG_S(INFO)` calls you make. Consider
publishing information messages to the `debug` channel that the SDK provides for you.
* Instead of logging acks, skip them and let another bot process them.<br><br>
For example, implement a single control bot that manages several video bots. The control bot
receives configuration updates from a frontend, publishes them to the control channel, and verifies that each video bot
has received them by looking for ack messages.<br>
When all the video bots have published an ack, the control bot publishes a second message that tells the video bots to move
the new configurations to the bot context.
* **Remember that messages published to the control must always contain the field `"to": "<bot_id>"` where `<bot_id>` is
the value of the `--id` parameter on the command line.** If you leave out this field, the SDK logs an error and
doesn't send the message to your control callback.

You've completed the video SDK tutorial. If you haven't already, read the [Satori Video SDK for C++ Concepts](concepts.md)
to learn more about the video SDK.
