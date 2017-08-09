cmake_minimum_required (VERSION 3.7)
project (librtmvideo VERSION 0.1 LANGUAGES CXX)

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
  SET(CMAKE_BUILD_TYPE "Debug")
ENDIF()

include(cmake/conan.cmake)

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

find_package(ZLIB REQUIRED)

add_library(rtmvideo
    src/base64.cpp
    src/cbor_json.cpp
    src/cbor_tools.cpp
    src/data.cpp
    src/flow_rtm_aggregator.cpp
    src/flow_json_decoder.cpp
    src/rtmclient.cpp
    src/rtmpacket.cpp
    src/sink_rtm.cpp
    src/source.cpp
    src/source_camera.cpp
    src/source_file.cpp
    src/source_replay.cpp
    src/proxy_replay.cpp
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
        ${ZLIB_LIBRARIES}
    )
target_link_libraries(rtmvideo PUBLIC dl)

target_include_directories(rtmvideo
    PRIVATE ${ZLIB_INCLUDE_DIRS}
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
