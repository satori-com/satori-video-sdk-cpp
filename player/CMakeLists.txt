cmake_minimum_required (VERSION 3.7)
project (verifier-bot VERSION 0.1 LANGUAGES CXX)

set(CMAKE_C_FLAGS_DEBUG "-DBOT_DEBUG")

link_directories(${LIBRTMVIDEO_LIB_DIRS})
add_executable(player src/main.cpp)
set_property(TARGET player PROPERTY CXX_STANDARD 14)
add_dependencies(player rtmvideo)
target_include_directories(player PRIVATE
        ${LIBRTMVIDEO_INCLUDE_DIRS}
        ${BASE_OPENCV_BOT_INCLUDE_DIRS})
target_link_libraries(player PRIVATE
        ${OpenCV_LIBS}
        ${LIBRTMVIDEO_LIBRARIES}
        ${BASE_OPENCV_BOT_LIBRARIES}
        CONAN_PKG::Opencv)
