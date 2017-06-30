cmake_minimum_required (VERSION 3.8)
project (librtmvideo VERSION 0.1 LANGUAGES CXX)

include(cmake/boost.cmake)
include(cmake/openssl.cmake)
include(cmake/rapidjson.cmake)
include(cmake/ffmpeg.cmake)
include(cmake/gsl.cmake)
include(cmake/beast.cmake)
include(cmake/libcbor.cmake)

add_library(rtmvideo SHARED
    src/decoder.cpp
    src/rtmclient.cpp
    src/tele.cpp
    src/cbor_cmd.cpp
    src/video_bot.cpp)
set_property(TARGET rtmvideo PROPERTY CXX_STANDARD 14)
add_dependencies(rtmvideo boost openssl rapidjson ffmpeg gsl beast project_libcbor)
target_link_libraries(rtmvideo
    PRIVATE ssl crypto boost_system boost_regex boost_program_options avcodec avutil iconv swscale ${LIBCBOR_LIBS})
target_include_directories(rtmvideo
    PUBLIC include ${LIBCBOR_INCLUDE_DIRS})

set(LIBRTMVIDEO_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/include ${LIBCBOR_INCLUDE_DIRS} PARENT_SCOPE)
set(LIBRTMVIDEO_LIBRARIES rtmvideo ${LIBCBOR_LIBRARIES} PARENT_SCOPE)
