cmake_minimum_required (VERSION 3.7)
project (librtmvideo VERSION 0.1 LANGUAGES CXX)

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
  SET(CMAKE_BUILD_TYPE "Debug")
ENDIF()

include(conan/conan.cmake)

conan_cmake_run(CONANFILE conanfile.py
                BASIC_SETUP CMAKE_TARGETS
                UPDATE
                BUILD missing)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(UNIX AND NOT APPLE)
    # Next line is needed when FFmpeg used as a static library
    # http://www.ffmpeg.org/platform.html#Advanced-linking-configuration
    set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic")
endif()

add_subdirectory(decoder)

add_library(rtmvideo
    src/base64.cpp
    src/cbor_json.cpp
    src/cbor_tools.cpp
    src/data.cpp
    src/flow_rtm_aggregator.cpp
    src/rtmclient.cpp
    src/rtmpacket.cpp
    src/sink_rtm.cpp
    src/source.cpp
    src/source_camera.cpp
    src/source_file.cpp
    src/source_replay.cpp
    src/source_rtm.cpp
    src/tele.cpp
    src/video_bot.cpp)
set_property(TARGET rtmvideo PROPERTY CXX_STANDARD 14)
target_link_libraries(rtmvideo
    PUBLIC
        CONAN_PKG::Openssl
        CONAN_PKG::Rapidjson
        CONAN_PKG::Libcbor
        CONAN_PKG::Ffmpeg
    PRIVATE
        CONAN_PKG::Boost
        CONAN_PKG::Gsl
        CONAN_PKG::Beast
        rtmvideo-decoder
    )
target_link_libraries(rtmvideo PUBLIC dl)

target_include_directories(rtmvideo
    PUBLIC include)

add_executable(rtmvideo_publisher_cli
    src/rtmvideo_publisher_cli.cpp)
set_property(TARGET rtmvideo_publisher_cli PROPERTY CXX_STANDARD 14)
set_property(TARGET rtmvideo_publisher_cli PROPERTY RUNTIME_OUTPUT_DIRECTORY bin)
add_dependencies(rtmvideo_publisher_cli rtmvideo)
target_include_directories(rtmvideo_publisher_cli PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(rtmvideo_publisher_cli
        PRIVATE
        rtmvideo
        CONAN_PKG::Boost
        CONAN_PKG::Ffmpeg
        CONAN_PKG::Openssl
        CONAN_PKG::Rapidjson
        CONAN_PKG::Gsl)

add_executable(test_bot
    src/test_bot.cpp)
set_property(TARGET test_bot PROPERTY CXX_STANDARD 14)
set_property(TARGET test_bot PROPERTY RUNTIME_OUTPUT_DIRECTORY bin)
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

enable_testing()
file(COPY test_data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME ReplayTest COMMAND bash -c "./bin/test_bot --replay_file=test_data/test.replay --config=test_data/config.json --analysis=a1.txt --debug=d1.txt && grep \"Hello from bot 1\" a1.txt && grep \"Hello from bot 2\" d1.txt")
add_test(NAME VideoTest COMMAND bash -c "./bin/test_bot --video_file=test_data/test.mp4 --config=test_data/config.json  --analysis=a2.txt --debug=d2.txt && grep \"Hello from bot 1\" a2.txt && grep \"Hello from bot 2\" d2.txt")
