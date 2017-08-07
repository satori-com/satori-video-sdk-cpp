cmake_minimum_required (VERSION 3.7)
project (librtmvideo VERSION 0.1 LANGUAGES CXX)

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
    src/flow_decoder.cpp
    src/flow_rtm_aggregator.cpp
    src/rtmclient.cpp
    src/rtmpacket.cpp
    src/sink_rtm.cpp
    src/source.cpp
    src/source_camera.cpp
    src/source_file.cpp
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
        CONAN_PKG::Opencv
        CONAN_PKG::Gsl
        CONAN_PKG::Beast
        rtmvideo-decoder
        ${ZLIB_LIBRARIES}
    )
target_link_libraries(rtmvideo PUBLIC dl)

target_include_directories(rtmvideo
    PRIVATE ${ZLIB_INCLUDE_DIRS}
    PUBLIC include)

set(LIBRTMVIDEO_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/include)
set(LIBRTMVIDEO_INCLUDE_DIRS ${LIBRTMVIDEO_INCLUDE_DIRS} PARENT_SCOPE)
set(LIBRTMVIDEO_LIBRARIES rtmvideo rtmvideo-decoder CONAN_PKG::Libcbor CONAN_PKG::Boost dl)
set(LIBRTMVIDEO_LIBRARIES ${LIBRTMVIDEO_LIBRARIES} PARENT_SCOPE)
set(LIBRTMVIDEO_LIB_DIRS ${LIBCBOR_LIB_DIR})
set(LIBRTMVIDEO_LIB_DIRS ${LIBRTMVIDEO_LIB_DIRS} PARENT_SCOPE)

link_directories(${LIBRTMVIDEO_LIB_DIRS})
add_executable(rtmvideo_publisher_cli
    src/rtmvideo_publisher_cli.cpp)
set_property(TARGET rtmvideo_publisher_cli PROPERTY CXX_STANDARD 14)
set_property(TARGET rtmvideo_publisher_cli PROPERTY RUNTIME_OUTPUT_DIRECTORY bin)
add_dependencies(rtmvideo_publisher_cli rtmvideo)
target_include_directories(rtmvideo_publisher_cli PRIVATE ${LIBRTMVIDEO_INCLUDE_DIRS})
target_link_libraries(rtmvideo_publisher_cli
        PRIVATE
        ${LIBRTMVIDEO_LIBRARIES}
        CONAN_PKG::Ffmpeg
        CONAN_PKG::Opencv
        CONAN_PKG::Gsl)

add_subdirectory(player)
