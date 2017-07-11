cmake_minimum_required (VERSION 3.7)
project (librtmvideo-decoder VERSION 0.1 LANGUAGES CXX)

if (NOT FFMPEG_FOUND)
    # include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/libvpx.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/ffmpeg.cmake)
    # add_dependencies(project_ffmpeg project_libvpx)
endif()

add_library(rtmvideo-decoder SHARED
    src/decoder.cpp
    )
target_compile_options(rtmvideo-decoder PRIVATE "-std=c++14")
set_property(TARGET rtmvideo-decoder PROPERTY CXX_STANDARD 14)
set_property(TARGET rtmvideo-decoder PROPERTY CXX_STANDARD_REQUIRED)
target_link_libraries(rtmvideo-decoder
    PRIVATE avcodec avutil swscale ${LIBVPX_LIBS})

if (APPLE)
    target_link_libraries(rtmvideo-decoder PRIVATE iconv)
endif()

target_include_directories(rtmvideo-decoder
    PRIVATE ${FFMPEG_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../include)

