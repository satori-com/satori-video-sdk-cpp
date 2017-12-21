cmake_minimum_required (VERSION 3.7)
project (satori-video-sdk-cpp VERSION 0.1 LANGUAGES CXX)

string(TIMESTAMP CMAKE_GEN_TIME UTC)

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
  SET(CMAKE_BUILD_TYPE "Debug")
ENDIF()

# Download automatically, you can also just copy the conan.cmake file
if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
  message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
  file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/master/conan.cmake"
                "${CMAKE_BINARY_DIR}/conan.cmake")
endif()
include(${CMAKE_BINARY_DIR}/conan.cmake)

conan_cmake_run(CONANFILE conanfile.py
                BASIC_SETUP CMAKE_TARGETS
                UPDATE
                BUILD outdated)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(UNIX AND NOT APPLE)
    # Next line is needed when FFmpeg used as a static library
    # http://www.ffmpeg.org/platform.html#Advanced-linking-configuration
    set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic")
endif()

if (DEFINED CMAKE_CXX_SANITIZER)
    message("** Using ${CMAKE_CXX_SANITIZER} sanitizer")
    set(CMAKE_CXX_SANITIZER_FLAGS "-fsanitize=${CMAKE_CXX_SANITIZER} -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_SANITIZER_FLAGS} ${CMAKE_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_CXX_SANITIZER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_CXX_SANITIZER_FLAGS} ${CMAKE_SHARED_LINKER_FLAGS}")
endif()

set(CMAKE_CXX_FLAGS "-pedantic-errors -ftemplate-backtrace-limit=99 ${CMAKE_CXX_FLAGS}")

add_library(satorivideo
    include/satorivideo/base.h
    include/satorivideo/video_bot.h
    include/satorivideo/multiframe/bot.h
    src/avutils.cpp
    src/base64.cpp
    src/bot_environment.cpp
    src/bot_instance_builder.cpp
    src/bot_instance.cpp
    src/camera_source.cpp
    src/cbor_json.cpp
    src/cbor_tools.cpp
    src/cli_streams.cpp
    src/data.cpp
    src/decode_image_frames.cpp
    src/file_source.cpp
    src/logging.h
    src/logging_impl.h
    src/metrics.cpp
    src/mkv_options.h
    src/mkv_sink.cpp
    src/pool_controller.h
    src/pool_controller.cpp
    src/replay_source.cpp
    src/rtm_client.cpp
    src/rtm_sink.cpp
    src/rtm_source.cpp
    src/rtm_streams.cpp
    src/satori_video.h
    src/signal_utils.cpp
    src/statsutils.cpp
    src/stopwatch.h
    src/streams/asio_streams.h
    src/streams/asio_streams_impl.h
    src/streams/channel.h
    src/streams/deferred.h
    src/streams/error_or.h
    src/streams/stream_error.cpp
    src/streams/streams.cpp
    src/streams/streams_impl.h
    src/streams/threaded_worker.h
    src/streams/type_traits.h
    src/streams/type_traits.h
    src/threadutils.cpp
    src/url_source.cpp
    src/version.cpp
    src/video_bot.cpp
    src/video_error.cpp
    src/video_streams.cpp
    src/vp9_encoder.cpp
    )
set_property(TARGET satorivideo PROPERTY CXX_STANDARD 14)
target_link_libraries(satorivideo
    PUBLIC
        dl
        CONAN_PKG::Gsl
        CONAN_PKG::Libcbor   # part of api interface
    PRIVATE
        CONAN_PKG::Beast
        CONAN_PKG::Boost
        CONAN_PKG::Ffmpeg
        CONAN_PKG::Json
        CONAN_PKG::Loguru
        CONAN_PKG::Openssl
        CONAN_PKG::PrometheusCpp
    )
target_include_directories(satorivideo PUBLIC include)
target_compile_definitions(satorivideo PRIVATE CONAN_PACKAGE_VERSION="${CONAN_PACKAGE_VERSION}")
target_compile_definitions(satorivideo PRIVATE CONAN_PACKAGE_NAME="${CONAN_PACKAGE_NAME}")
target_compile_definitions(satorivideo PRIVATE CONAN_SETTINGS_ARCH="${CONAN_SETTINGS_ARCH}")
target_compile_definitions(satorivideo PRIVATE CONAN_SETTINGS_BUILD_TYPE="${CONAN_SETTINGS_BUILD_TYPE}")
target_compile_definitions(satorivideo PRIVATE CONAN_SETTINGS_COMPILER="${CONAN_SETTINGS_COMPILER}")
target_compile_definitions(satorivideo PRIVATE CONAN_SETTINGS_COMPILER_LIBCXX="${CONAN_SETTINGS_COMPILER_LIBCXX}")
target_compile_definitions(satorivideo PRIVATE CONAN_SETTINGS_COMPILER_VERSION="${CONAN_SETTINGS_COMPILER_VERSION}")
target_compile_definitions(satorivideo PRIVATE CONAN_SETTINGS_OS="${CONAN_SETTINGS_OS}")
target_compile_definitions(satorivideo PRIVATE GIT_COMMIT_HASH="${GIT_COMMIT_HASH}")
target_compile_definitions(satorivideo PRIVATE CMAKE_GEN_TIME="${CMAKE_GEN_TIME}")


if (CONAN_GPERFTOOLS_ROOT)
    message("** Enabling gperftools")
    target_link_libraries(satorivideo PUBLIC CONAN_PKG::GPerfTools)
    add_definitions(-DHAS_GPERFTOOLS)
endif()

if (CONAN_OPENCV_ROOT)
    message("** Building OpenCV variant")
    target_link_libraries(satorivideo PUBLIC CONAN_PKG::Opencv)
    target_sources(satorivideo PRIVATE
            include/satorivideo/opencv/opencv_bot.h
            include/satorivideo/opencv/opencv_utils.h
            src/opencv/opencv_bot.cpp
            src/opencv/opencv_utils.cpp
            )
endif()

function(set_binary_output_directory TARGET DIRECTORY)
    set_property(TARGET ${TARGET} PROPERTY
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${DIRECTORY})
    set_property(TARGET ${TARGET} PROPERTY
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/${DIRECTORY})
    set_property(TARGET ${TARGET} PROPERTY
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/${DIRECTORY})
    set_property(TARGET ${TARGET} PROPERTY
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/${DIRECTORY})
    set_property(TARGET ${TARGET} PROPERTY
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/${DIRECTORY})
endfunction()

add_executable(satori_video_publisher src/clitools/publisher.cpp)
set_property(TARGET satori_video_publisher PROPERTY CXX_STANDARD 14)
set_binary_output_directory(satori_video_publisher bin)
add_dependencies(satori_video_publisher satorivideo)
target_include_directories(satori_video_publisher PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(satori_video_publisher
        PRIVATE
        satorivideo
        CONAN_PKG::Boost
        CONAN_PKG::Ffmpeg
        CONAN_PKG::Gsl
        CONAN_PKG::Json
        CONAN_PKG::Loguru
        CONAN_PKG::Openssl
        CONAN_PKG::PrometheusCpp
        )

add_executable(satori_video_recorder src/clitools/recorder.cpp)
set_property(TARGET satori_video_recorder PROPERTY CXX_STANDARD 14)
set_binary_output_directory(satori_video_recorder bin)
add_dependencies(satori_video_recorder satorivideo)
target_include_directories(satori_video_recorder PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(satori_video_recorder
        PRIVATE
        satorivideo
        CONAN_PKG::Boost
        CONAN_PKG::Ffmpeg
        CONAN_PKG::Gsl
        CONAN_PKG::Json
        CONAN_PKG::Loguru
        CONAN_PKG::Openssl
        CONAN_PKG::PrometheusCpp
        )

add_executable(satori_video_player src/clitools/player.cpp)
set_property(TARGET satori_video_player PROPERTY CXX_STANDARD 14)
set_binary_output_directory(satori_video_player bin)
add_dependencies(satori_video_player satorivideo)
target_include_directories(satori_video_player PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(satori_video_player
        PRIVATE
        satorivideo
        CONAN_PKG::Boost
        CONAN_PKG::Ffmpeg
        CONAN_PKG::Gsl
        CONAN_PKG::Json
        CONAN_PKG::Loguru
        CONAN_PKG::Openssl
        CONAN_PKG::PrometheusCpp
        CONAN_PKG::SDL
        )

add_executable(test_configure_bot test/bots/test_configure_bot.cpp)
set_property(TARGET test_configure_bot PROPERTY CXX_STANDARD 14)
set_binary_output_directory(test_configure_bot test)
add_dependencies(test_configure_bot satorivideo)
target_include_directories(test_configure_bot PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(test_configure_bot
        PRIVATE
        satorivideo
        CONAN_PKG::Boost
        CONAN_PKG::Ffmpeg
        CONAN_PKG::Gsl
        CONAN_PKG::Json
        CONAN_PKG::Loguru
        CONAN_PKG::Openssl
        )


add_executable(test_bot test/bots/test_bot.cpp)
set_property(TARGET test_bot PROPERTY CXX_STANDARD 14)
set_binary_output_directory(test_bot test)
target_link_libraries(test_bot PRIVATE satorivideo CONAN_PKG::Loguru)

add_executable(empty_bot test/bots/empty_bot.cpp)
set_property(TARGET empty_bot PROPERTY CXX_STANDARD 14)
set_binary_output_directory(empty_bot test)
target_link_libraries(empty_bot PRIVATE satorivideo CONAN_PKG::Loguru)

if (CONAN_OPENCV_ROOT)
    add_executable(empty_opencv_bot test/bots/empty_opencv_bot.cpp)
    set_property(TARGET empty_opencv_bot PROPERTY CXX_STANDARD 14)
    set_binary_output_directory(empty_opencv_bot test)
    target_link_libraries(empty_opencv_bot PRIVATE satorivideo CONAN_PKG::Loguru)

    add_executable(test_opencv_bot test/bots/test_opencv_bot.cpp)
    set_property(TARGET test_opencv_bot PROPERTY CXX_STANDARD 14)
    set_binary_output_directory(test_opencv_bot test)
    target_link_libraries(test_opencv_bot PRIVATE satorivideo)
endif()

function(add_video_test TEST_NAME TEST_FILE)
    add_executable(${TEST_NAME} ${TEST_FILE})
    set_property(TARGET ${TEST_NAME} PROPERTY CXX_STANDARD 14)
    set_binary_output_directory(${TEST_NAME} test)
    add_dependencies(${TEST_NAME} satorivideo)
    target_include_directories(${TEST_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(${TEST_NAME}
        PRIVATE
            satorivideo
            CONAN_PKG::Boost
            CONAN_PKG::Ffmpeg
            CONAN_PKG::Gsl
            CONAN_PKG::Json
            CONAN_PKG::Loguru
            CONAN_PKG::Openssl
            CONAN_PKG::PrometheusCpp
        )
    add_test(${TEST_NAME} ${CMAKE_BINARY_DIR}/test/${TEST_NAME})
endfunction()

enable_testing()

file(COPY test_data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME ReplayTest COMMAND bash -c "\
${CMAKE_BINARY_DIR}/test/test_configure_bot \
--input-replay-file=test_data/test.replay \
--config-file=test_data/config.json \
--analysis-file=a1.txt \
--debug=d1.txt \
-v 9 \
&& grep test_analysis_message a1.txt \
&& grep test_debug_message d1.txt")

add_test(NAME VideoTest COMMAND bash -c "\
${CMAKE_BINARY_DIR}/test/test_configure_bot \
--input-video-file=test_data/test.mp4 \
--config-file=test_data/config.json \
--analysis-file=a2.txt \
--debug=d2.txt \
&& grep test_analysis_message a2.txt \
&& grep test_debug_message d2.txt")

add_test(NAME ReplayBatchTest COMMAND bash -c "\
${CMAKE_BINARY_DIR}/test/test_configure_bot \
--batch \
--input-replay-file=test_data/test.replay \
--config-file=test_data/config.json \
--analysis-file=a3.txt \
--debug=d3.txt \
&& grep test_analysis_message a3.txt \
&& grep test_debug_message d3.txt")

add_test(NAME VideoBatchTest COMMAND bash -c "\
${CMAKE_BINARY_DIR}/test/test_configure_bot \
--batch \
--input-video-file=test_data/test.mp4 \
--config-file=test_data/config.json \
--analysis-file=a4.txt \
--debug=d4.txt \
&& grep test_analysis_message a4.txt \
&& grep test_debug_message d4.txt")

add_test(NAME ReplayTestConfigString COMMAND bash -c "\
${CMAKE_BINARY_DIR}/test/test_configure_bot \
--input-replay-file=test_data/test.replay \
--config='{\"myparam\":\"myvalue\"}' \
--analysis-file=a5.txt \
--debug=d5.txt \
-v 9 \
&& grep test_analysis_message a5.txt \
&& grep test_debug_message d5.txt")

add_test(NAME ShutdownTest COMMAND bash -c "\
${CMAKE_BINARY_DIR}/test/test_configure_bot \
--input-video-file=test_data/test.mp4 \
--config-file=test_data/config.json \
--analysis-file=test_shutdown_analysis.txt \
--debug=test_shutdown_debug.txt \
| tee /dev/tty \
| grep 'bot is shutting down' \
&& grep test_analysis_message test_shutdown_analysis.txt \
&& grep test_debug_message test_shutdown_debug.txt \
&& grep test_shutdown_message test_shutdown_debug.txt")

add_test(NAME DefaultResolutionTest COMMAND bash -c "\
${CMAKE_BINARY_DIR}/test/test_bot \
--input-video-file=test_data/test.mp4 \
| tee /dev/tty \
| grep 'got frame 320x240'")

add_test(NAME OriginalResolutionTest COMMAND bash -c "\
${CMAKE_BINARY_DIR}/test/test_bot \
--input-video-file=test_data/test.mp4 \
--input-resolution=original \
| tee /dev/tty \
| grep 'got frame 640x480'")

add_test(NAME CustomResolutionTest COMMAND bash -c "\
${CMAKE_BINARY_DIR}/test/test_bot \
--input-video-file=test_data/test.mp4 \
--input-resolution=160x130 \
| tee /dev/tty \
| grep 'got frame 160x120'")

add_test(NAME CustomResolutionLoseProportionsTest COMMAND bash -c "\
${CMAKE_BINARY_DIR}/test/test_bot \
--input-video-file=test_data/test.mp4 \
--input-resolution=160x130 \
--keep-proportions=false \
| tee /dev/tty \
| grep 'got frame 160x130'")

add_test(NAME ProcessEmptyConfigTest COMMAND bash -c "\
${CMAKE_BINARY_DIR}/test/test_bot \
--input-video-file=test_data/test.mp4 \
| tee /dev/tty \
| grep 'got no config'")

add_test(NAME ProcessNonemptyConfigTest COMMAND bash -c "\
${CMAKE_BINARY_DIR}/test/test_bot \
--input-video-file=test_data/test.mp4 \
--config='{\"myparam\":\"myvalue\"}' \
| tee /dev/tty \
| grep 'processing config {\"myparam\":\"myvalue\"}'")

# flaky test
# add_test(NAME FramesLimitTest COMMAND bash -c "${CMAKE_BINARY_DIR}/test/empty_bot --input-video-file=test_data/test.mp4 --frames-limit=5 | tee /dev/tty | grep -o 'got frame 320x240' | wc -l | tee /dev/tty | grep '5'")

if (CONAN_OPENCV_ROOT)
add_test(NAME OpencvTest COMMAND bash -c "\
${CMAKE_BINARY_DIR}/test/test_opencv_bot \
--input-video-file=test_data/test.mp4 \
| tee /dev/tty \
| grep 'got frame 240 x 320'")
endif()

add_video_test(avutils_test test/avutils_test.cpp)
add_video_test(deferred_test test/deferred_test.cpp)
add_video_test(error_or_test test/error_or_test.cpp)
add_video_test(file_source_test test/file_source_test.cpp)
add_video_test(decode_image_frames_test test/decode_image_frames_test.cpp)
add_video_test(streams_test test/streams_test.cpp)
add_video_test(vp9_encoder_test test/vp9_encoder_test.cpp)
add_video_test(cbor_tools_test test/cbor_tools_test.cpp)
add_video_test(data_test test/data_test.cpp)
add_video_test(encoding_test test/encoding_test.cpp)
add_video_test(threadutils_test test/threadutils_test.cpp)
add_video_test(bot_instance_test test/bot_instance_test.cpp)
add_video_test(cbor_to_json_test test/cbor_to_json_test.cpp)
add_video_test(json_to_cbor_test test/json_to_cbor_test.cpp)
