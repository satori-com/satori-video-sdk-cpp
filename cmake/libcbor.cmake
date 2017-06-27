# libcbor is a C library for parsing and generating CBOR, the general-purpose schema-less binary data format

set(LIBCBOR_PREFIX ${CMAKE_BINARY_DIR}/libcbor)
set(LIBCBOR_LIBS libcbor.a)

ExternalProject_Add(
        libcbor
        PREFIX ${LIBCBOR_PREFIX}
        GIT_REPOSITORY https://github.com/PJK/libcbor.git
        GIT_TAG 6ffbfc41559a6ef5a3a18a0ffe005d19eb7fd528 # v0.5.0
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
)

include_directories(${LIBCBOR_PREFIX}/src/libcbor/include/)
