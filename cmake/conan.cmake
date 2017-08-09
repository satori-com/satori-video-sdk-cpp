set(CMAKE_CONAN_VERSION df45ac513c31e73ec1c401c2264f50d4c0ce2f9b)

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan-${CMAKE_CONAN_VERSION}.cmake")
    message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
    file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/${CMAKE_CONAN_VERSION}/conan.cmake"
            "${CMAKE_BINARY_DIR}/conan-${CMAKE_CONAN_VERSION}.cmake")
endif()

include(${CMAKE_BINARY_DIR}/conan-${CMAKE_CONAN_VERSION}.cmake)
