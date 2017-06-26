# libcbor is a C library for parsing and generating CBOR, the general-purpose schema-less binary data format

set(LIBCBOR_PREFIX ${CMAKE_BINARY_DIR}/libcbor)
set(LIBCBOR_LIBS libcbor.a)

ExternalProject_Add(
        libcbor
        PREFIX ${LIBCBOR_PREFIX}
        GIT_REPOSITORY https://github.com/PJK/libcbor.git
        GIT_TAG 076b491e70cdf6557299727be69f5c44eaa4d7c6
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
)

include_directories(${LIBCBOR_PREFIX}/src/libcbor/include/)
