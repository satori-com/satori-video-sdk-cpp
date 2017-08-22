project(PackageTest CXX)
cmake_minimum_required(VERSION 2.8.12)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_executable(test test.cpp)
set_property(TARGET test PROPERTY CXX_STANDARD 14)
target_link_libraries(test ${CONAN_LIBS})
