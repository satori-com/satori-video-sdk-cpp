# libvpx is vp8/vp9 encoder/decoder
include(ExternalProject)

set(LIBVPX_PREFIX ${CMAKE_BINARY_DIR}/libvpx)
set(LIBVPX_LIBS libvpx)

set(LIBVPX_CONFIGURE_ARGS --prefix=${LIBVPX_PREFIX}/install/
        --enable-pic --disable-examples --disable-tools --disable-docs
        --disable-multithread)

set(LIBVPX_CONFIGURE_COMMAND ./configure)
set(LIBVPX_MAKE_COMMAND make)
set(LIBVPX_LIB_SUFFIX a)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    message("    libvpx: Emscripten")
    set(LIBVPX_CONFIGURE_ARGS ${LIBVPX_CONFIGURE_ARGS}
            --target=generic-gnu  --disable-static --enable-shared)
    set(LIBVPX_CONFIGURE_COMMAND emconfigure ./configure)
    set(LIBVPX_MAKE_COMMAND emmake make)
    set(LIBVPX_LIB_SUFFIX so)
ENDIF()

IF(CMAKE_BUILD_TYPE MATCHES Debug)
    message("    libvpx: Debug")
    set(LIBVPX_CONFIGURE_ARGS ${LIBVPX_CONFIGURE_ARGS}
            --disable-optimizations --enable-debug)
ENDIF()

ExternalProject_Add(
        project_libvpx
        PREFIX ${LIBVPX_PREFIX}
        GIT_REPOSITORY https://chromium.googlesource.com/webm/libvpx
        GIT_TAG v1.6.1
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ${LIBVPX_CONFIGURE_COMMAND} ${LIBVPX_CONFIGURE_ARGS}
        BUILD_COMMAND V=1 ${LIBVPX_MAKE_COMMAND} -j8
        INSTALL_COMMAND make install
        BUILD_BYPRODUCTS ${LIBVPX_PREFIX}/install/lib/libvpx.${LIBVPX_LIB_SUFFIX}
        BUILD_IN_SOURCE 1
)

set(LIBVPX_LIB_DIR ${LIBVPX_PREFIX}/install/lib/)
set(LIBVPX_INCLUDE_DIR ${LIBVPX_PREFIX}/install/include/)

add_library(libvpx STATIC IMPORTED)
add_dependencies(libvpx project_libvpx)
set_property(TARGET libvpx PROPERTY IMPORTED_LOCATION ${LIBVPX_PREFIX}/install/lib/libvpx.${LIBVPX_LIB_SUFFIX})
set(LIBVPX_LIBS libvpx)
