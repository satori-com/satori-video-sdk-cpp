project(PackageTest LANGUAGES CXX)
cmake_minimum_required(VERSION 2.8.12)

set(CMAKE_CXX_STANDARD 14)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_executable(test test.cpp)
target_link_libraries(test ${CONAN_LIBS} z dl)
