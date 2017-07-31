cmake_minimum_required (VERSION 3.7)
project (librtmvideo-decoder VERSION 0.1 LANGUAGES CXX)

if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    message("*** BUILDING JS DECODER!!!")
    include("${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    conan_basic_setup(TARGETS)
endif()

add_library(rtmvideo-decoder
    src/base64.cpp
    src/decoder.cpp
    )
target_compile_options(rtmvideo-decoder PRIVATE "-std=c++14")
set_property(TARGET rtmvideo-decoder PROPERTY CXX_STANDARD 14)
set_property(TARGET rtmvideo-decoder PROPERTY CXX_STANDARD_REQUIRED)
target_link_libraries(rtmvideo-decoder
    PRIVATE CONAN_PKG::Ffmpeg)

target_include_directories(rtmvideo-decoder
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

