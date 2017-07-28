cmake_minimum_required (VERSION 3.7)
project (librtmvideo VERSION 0.1 LANGUAGES CXX)

if(UNIX AND NOT APPLE)
    # Next line is needed when FFmpeg used as a static library
    # http://www.ffmpeg.org/platform.html#Advanced-linking-configuration
    set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic")
endif()

include(cmake/libvpx.cmake)
include(cmake/ffmpeg.cmake)

add_dependencies(project_ffmpeg project_libvpx)

add_subdirectory(decoder)

find_package(ZLIB REQUIRED)

add_library(rtmvideo SHARED
    src/base64.cpp
    src/cbor_json.cpp
    src/cbor_map.cpp
    src/data.cpp
    src/rtmclient.cpp
    src/rtmpacket.cpp
    src/sink_rtm.cpp
    src/source.cpp
    src/source_camera.cpp
    src/source_file.cpp
    src/tele.cpp
    src/video_bot.cpp)
set_property(TARGET rtmvideo PROPERTY CXX_STANDARD 14)
target_link_libraries(rtmvideo
    PUBLIC
        CONAN_PKG::Openssl
        CONAN_PKG::Rapidjson
        CONAN_PKG::Libcbor
    PRIVATE
        CONAN_PKG::Boost
        CONAN_PKG::Opencv
        CONAN_PKG::Gsl
        CONAN_PKG::Beast
        avdevice avformat swscale rtmvideo-decoder
        ${ZLIB_LIBRARIES}
    )
target_link_libraries(rtmvideo PUBLIC avcodec avutil ${LIBVPX_LIBS} dl)

if(APPLE)
    target_link_libraries(rtmvideo PRIVATE
    iconv
    "-framework AVFoundation"
    "-framework CoreGraphics"
    "-framework CoreMedia"
    "-framework Foundation"
    "-framework QuartzCore")
endif()

target_include_directories(rtmvideo
    PRIVATE ${FFMPEG_INCLUDE_DIR} ${ZLIB_INCLUDE_DIRS}
    PUBLIC include)

set(LIBRTMVIDEO_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/include ${FFMPEG_INCLUDE_DIR})
set(LIBRTMVIDEO_INCLUDE_DIRS ${LIBRTMVIDEO_INCLUDE_DIRS} PARENT_SCOPE)
set(LIBRTMVIDEO_LIBRARIES rtmvideo rtmvideo-decoder pthread vpx CONAN_PKG::Libcbor CONAN_PKG::Boost dl)
set(LIBRTMVIDEO_LIBRARIES ${LIBRTMVIDEO_LIBRARIES} PARENT_SCOPE)
set(LIBRTMVIDEO_LIB_DIRS ${LIBCBOR_LIB_DIR} ${LIBVPX_LIB_DIR})
set(LIBRTMVIDEO_LIB_DIRS ${LIBRTMVIDEO_LIB_DIRS} PARENT_SCOPE)

link_directories(${LIBRTMVIDEO_LIB_DIRS})
add_executable(rtmvideo_publisher_cli
    src/rtmvideo_publisher_cli.cpp)
set_property(TARGET rtmvideo_publisher_cli PROPERTY CXX_STANDARD 14)
set_property(TARGET rtmvideo_publisher_cli PROPERTY RUNTIME_OUTPUT_DIRECTORY bin)
add_dependencies(rtmvideo_publisher_cli rtmvideo)
target_include_directories(rtmvideo_publisher_cli PRIVATE ${LIBRTMVIDEO_INCLUDE_DIRS})
target_link_libraries(rtmvideo_publisher_cli PRIVATE ${LIBRTMVIDEO_LIBRARIES} CONAN_PKG::Opencv CONAN_PKG::Gsl)

add_subdirectory(player)
