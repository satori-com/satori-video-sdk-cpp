# libvpx is vp8/vp9 encoder/decoder

set(LIBVPX_PREFIX ${CMAKE_BINARY_DIR}/libvpx)
set(LIBVPX_LIBS libvpx)

ExternalProject_Add(
        project_libvpx
        PREFIX ${LIBVPX_PREFIX}
        GIT_REPOSITORY https://chromium.googlesource.com/webm/libvpx
        GIT_TAG v1.6.1
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ./configure --prefix=${LIBVPX_PREFIX}/install/ --enable-pic
        BUILD_COMMAND make -j8
        INSTALL_COMMAND make install
        BUILD_BYPRODUCTS ${LIBVPX_PREFIX}/install/lib/libvpx.a
        BUILD_IN_SOURCE 1
)

set(LIBVPX_LIB_DIR ${LIBVPX_PREFIX}/install/lib/)
set(LIBVPX_INCLUDE_DIR ${LIBVPX_PREFIX}/install/include/)

add_library(libvpx STATIC IMPORTED)
add_dependencies(libvpx project_libvpx)
set_property(TARGET libvpx PROPERTY IMPORTED_LOCATION ${LIBVPX_PREFIX}/install/lib/libvpx.a)
set(LIBVPX_LIBS libvpx)
