cmake_minimum_required (VERSION 3.8)
project (librtmvideo VERSION 0.1 LANGUAGES CXX)

include(cmake/boost.cmake)
include(cmake/openssl.cmake)
include(cmake/rapidjson.cmake)
include(cmake/ffmpeg.cmake)
include(cmake/gsl.cmake)
include(cmake/beast.cmake)

add_library(rtmvideo SHARED
    src/decoder.cpp
    src/rtmclient.cpp
    src/video_bot.cpp)
set_property(TARGET rtmvideo PROPERTY CXX_STANDARD 14)
add_dependencies(rtmvideo boost openssl rapidjson ffmpeg gsl beast)
target_link_libraries(rtmvideo 
    PRIVATE ssl crypto boost_system boost_regex boost_program_options avcodec avutil iconv swscale)
target_include_directories(rtmvideo
    PUBLIC include)
