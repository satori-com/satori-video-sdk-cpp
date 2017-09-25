cmake_minimum_required (VERSION 3.7)
project (librtmvideo VERSION 0.1 LANGUAGES CXX)

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
  SET(CMAKE_BUILD_TYPE "Debug")
ENDIF()

include(conan/conan.cmake)

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
    set(CMAKE_CXX_FLAGS "-fsanitize=${CMAKE_CXX_SANITIZER} ${CMAKE_CXX_FLAGS}")
endif()

set(CMAKE_CXX_FLAGS "-pedantic-errors -ftemplate-backtrace-limit=0 ${CMAKE_CXX_FLAGS}")

add_subdirectory(decoder)

add_library(rtmvideo
    src/avutils.cpp
    src/base64.cpp
    src/bot_environment.cpp
    src/bot_instance.cpp
    src/camera_source.cpp
    src/cbor_json.cpp
    src/cbor_tools.cpp
    src/cli_streams.cpp
    src/data.cpp
    src/decode_image_frames.cpp
    src/error.cpp
    src/file_source.cpp
    src/mkv_sink.cpp
    src/replay_source.cpp
    src/rtm_sink.cpp
    src/rtm_source.cpp
    src/rtm_streams.cpp
    src/rtmclient.cpp
    src/signal_breaker.cpp
    src/streams.cpp
    src/tele.cpp
    src/video_bot.cpp
    src/video_streams.cpp
    src/vp9_encoder.cpp
    )
set_property(TARGET rtmvideo PROPERTY CXX_STANDARD 14)
target_link_libraries(rtmvideo
    PUBLIC
        CONAN_PKG::Openssl
        CONAN_PKG::Rapidjson
        CONAN_PKG::Libcbor
        CONAN_PKG::Ffmpeg
    PRIVATE
        rtmvideo-decoder
        CONAN_PKG::Boost
        CONAN_PKG::Gsl
        CONAN_PKG::Beast
        CONAN_PKG::Loguru
    )
target_link_libraries(rtmvideo PUBLIC dl)

target_include_directories(rtmvideo
    PUBLIC include)

function(set_binary_output_directory TARGET DIRECTORY)
    set_property(TARGET ${TARGET} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${DIRECTORY})
    set_property(TARGET ${TARGET} PROPERTY RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/${DIRECTORY})
    set_property(TARGET ${TARGET} PROPERTY RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/${DIRECTORY})
endfunction()

add_executable(rtmvideo_publisher_cli
    src/rtmvideo_publisher_cli.cpp)
set_property(TARGET rtmvideo_publisher_cli PROPERTY CXX_STANDARD 14)
set_binary_output_directory(rtmvideo_publisher_cli bin)
add_dependencies(rtmvideo_publisher_cli rtmvideo)
target_include_directories(rtmvideo_publisher_cli PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(rtmvideo_publisher_cli
        PRIVATE
        rtmvideo
        CONAN_PKG::Boost
        CONAN_PKG::Ffmpeg
        CONAN_PKG::Loguru
        CONAN_PKG::Openssl
        CONAN_PKG::Rapidjson
        CONAN_PKG::Gsl)

add_executable(rtmvideo_recorder_cli
    src/rtmvideo_recorder_cli.cpp)
set_property(TARGET rtmvideo_recorder_cli PROPERTY CXX_STANDARD 14)
set_binary_output_directory(rtmvideo_recorder_cli bin)
add_dependencies(rtmvideo_recorder_cli rtmvideo)
target_include_directories(rtmvideo_recorder_cli PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(rtmvideo_recorder_cli
        PRIVATE
        rtmvideo
        CONAN_PKG::Boost
        CONAN_PKG::Ffmpeg
        CONAN_PKG::Loguru
        CONAN_PKG::Openssl
        CONAN_PKG::Rapidjson
        CONAN_PKG::Gsl
        CONAN_PKG::Loguru
        )

add_executable(player src/player.cpp)
set_property(TARGET player PROPERTY CXX_STANDARD 14)
set_binary_output_directory(player bin)
add_dependencies(player rtmvideo)
target_include_directories(player PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(player
        PRIVATE
        rtmvideo
        CONAN_PKG::Boost
        CONAN_PKG::Gsl
        CONAN_PKG::Loguru
        CONAN_PKG::SDL)

add_executable(test_bot
    test/test_bot.cpp)
set_property(TARGET test_bot PROPERTY CXX_STANDARD 14)
set_binary_output_directory(test_bot test)
add_dependencies(test_bot rtmvideo)
target_include_directories(test_bot PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_bot
        PRIVATE
        rtmvideo
        CONAN_PKG::Boost
        CONAN_PKG::Ffmpeg
        CONAN_PKG::Openssl
        CONAN_PKG::Rapidjson
        CONAN_PKG::Gsl)

add_executable(empty_bot test/empty_bot.cpp)
set_property(TARGET empty_bot PROPERTY CXX_STANDARD 14)
set_binary_output_directory(empty_bot test)
target_link_libraries(empty_bot PRIVATE rtmvideo)

function(add_rtmvideo_test TEST_NAME TEST_FILE)
    add_executable(${TEST_NAME} ${TEST_FILE})
    set_property(TARGET ${TEST_NAME} PROPERTY CXX_STANDARD 14)
    set_binary_output_directory(${TEST_NAME} test)
    add_dependencies(${TEST_NAME} rtmvideo)
    target_include_directories(${TEST_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)
    target_include_directories(${TEST_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(${TEST_NAME}
        PRIVATE
            rtmvideo
            CONAN_PKG::Boost
            CONAN_PKG::Loguru
            CONAN_PKG::Gsl
        )
    add_test(${TEST_NAME} ${CMAKE_BINARY_DIR}/test/${TEST_NAME} )
endfunction()

enable_testing()
file(COPY test_data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME ReplayTest COMMAND bash -c "${CMAKE_BINARY_DIR}/test/test_bot --input-replay-file=test_data/test.replay --config=test_data/config.json --analysis_file=a1.txt --debug=d1.txt -v 9 && grep test_analysis_message a1.txt && grep test_debug_message d1.txt")
add_test(NAME VideoTest COMMAND bash -c "${CMAKE_BINARY_DIR}/test/test_bot --input-video-file=test_data/test.mp4 --config=test_data/config.json  --analysis_file=a2.txt --debug=d2.txt && grep test_analysis_message a2.txt && grep test_debug_message d2.txt")
add_test(NAME ReplayBatchTest COMMAND bash -c "${CMAKE_BINARY_DIR}/test/test_bot --batch --input-replay-file=test_data/test.replay --config=test_data/config.json --analysis_file=a3.txt --debug=d3.txt && grep test_analysis_message a3.txt && grep test_debug_message d3.txt")
add_test(NAME VideoBatchTest COMMAND bash -c "${CMAKE_BINARY_DIR}/test/test_bot --batch --input-video-file=test_data/test.mp4 --config=test_data/config.json  --analysis_file=a4.txt --debug=d4.txt && grep test_analysis_message a4.txt && grep test_debug_message d4.txt")
add_rtmvideo_test(avutils_test test/avutils_test.cpp)
add_rtmvideo_test(streams_test test/streams_test.cpp)
add_rtmvideo_test(file_source_test test/file_source_test.cpp)
add_rtmvideo_test(flow_decoder_test test/flow_decoder_test.cpp)
add_rtmvideo_test(vp9_encoder_test test/vp9_encoder_test.cpp)
add_rtmvideo_test(producer_consumer_queue_test test/producer_consumer_queue_test.cpp)