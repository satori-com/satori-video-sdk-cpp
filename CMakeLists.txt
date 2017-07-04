cmake_minimum_required (VERSION 3.7)
project (librtmvideo VERSION 0.1 LANGUAGES CXX)

set(ICONV_LIBRARY "")

if(APPLE)
    set(ICONV_LIBRARY iconv)
endif()

if(UNIX AND NOT APPLE)
    # Next line is needed when FFmpeg used as a static library
    # http://www.ffmpeg.org/platform.html#Advanced-linking-configuration
    set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic")
endif()

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
    src/cbor_map.cpp
    src/video_bot.cpp)
set_property(TARGET rtmvideo PROPERTY CXX_STANDARD 14)
add_dependencies(rtmvideo boost openssl rapidjson ffmpeg gsl beast project_libcbor)
target_link_libraries(rtmvideo
    PRIVATE ssl crypto boost_system boost_regex boost_program_options avcodec avutil swscale ${ICONV_LIBRARY} ${LIBCBOR_LIBS})
target_include_directories(rtmvideo
    PUBLIC include ${LIBCBOR_INCLUDE_DIRS})

set(LIBRTMVIDEO_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/include ${LIBCBOR_INCLUDE_DIRS} PARENT_SCOPE)
set(LIBRTMVIDEO_LIBRARIES rtmvideo pthread dl cbor PARENT_SCOPE)
set(LIBRTMVIDEO_LIB_DIRS ${LIBCBOR_LIB_DIRS} PARENT_SCOPE)
