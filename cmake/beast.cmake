# HTTP and WebSocket built on Boost.Asio in C++11 

set(BEAST_PREFIX ${CMAKE_BINARY_DIR}/beast)

ExternalProject_Add(
        beast
        PREFIX ${BEAST_PREFIX}
        GIT_REPOSITORY https://github.com/vinniefalco/Beast.git
        GIT_TAG 4c15db48488cf292af76a8f4509686306b76449f # version 61
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
)

include_directories(${BEAST_PREFIX}/src/beast/include/)