cmake_minimum_required (VERSION 3.7)
project (librtmvideo VERSION 0.1 LANGUAGES CXX)

if(UNIX AND NOT APPLE)
    # Next line is needed when FFmpeg used as a static library
    # http://www.ffmpeg.org/platform.html#Advanced-linking-configuration
    set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic")
endif()

include(cmake/openssl.cmake)
include(cmake/rapidjson.cmake)
include(cmake/libvpx.cmake)
include(cmake/ffmpeg.cmake)
include(cmake/gsl.cmake)
include(cmake/beast.cmake)

add_dependencies(project_ffmpeg project_libvpx)

add_subdirectory(decoder)

find_package(ZLIB REQUIRED)

add_library(rtmvideo SHARED
    src/rtmclient.cpp
    src/cbor_json.cpp
    src/tele.cpp
    src/cbor_map.cpp
    src/video_bot.cpp
    src/video_source.cpp
    src/video_source_file.cpp
    src/video_source_camera.cpp)
set_property(TARGET rtmvideo PROPERTY CXX_STANDARD 14)
add_dependencies(rtmvideo openssl rapidjson gsl beast)
target_link_libraries(rtmvideo
    PRIVATE ssl crypto avdevice avformat swscale rtmvideo-decoder
    ${ZLIB_LIBRARIES} CONAN_PKG::Libcbor CONAN_PKG::Boost CONAN_PKG::Opencv)
target_link_libraries(rtmvideo PUBLIC avcodec avutil ${LIBVPX_LIBS})

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
    PRIVATE ${LIBCBOR_INCLUDE_DIRS} ${BOOST_INCLUDE_DIRS} ${FFMPEG_INCLUDE_DIR} ${ZLIB_INCLUDE_DIRS}
    PUBLIC include)

set(LIBRTMVIDEO_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/include ${LIBCBOR_INCLUDE_DIRS} ${BOOST_INCLUDE_DIRS} ${FFMPEG_INCLUDE_DIR})
set(LIBRTMVIDEO_INCLUDE_DIRS ${LIBRTMVIDEO_INCLUDE_DIRS} PARENT_SCOPE)
set(LIBRTMVIDEO_LIBRARIES rtmvideo rtmvideo-decoder pthread dl vpx CONAN_PKG::Libcbor CONAN_PKG::Boost)
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
target_link_libraries(rtmvideo_publisher_cli PRIVATE ${LIBRTMVIDEO_LIBRARIES}) 

add_subdirectory(player)
