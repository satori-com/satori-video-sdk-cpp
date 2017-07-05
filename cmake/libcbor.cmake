# libcbor is a C library for parsing and generating CBOR, the general-purpose schema-less binary data format

set(LIBCBOR_PREFIX ${CMAKE_BINARY_DIR}/libcbor)
set(LIBCBOR_LIBS libcbor)

ExternalProject_Add(
        project_libcbor
        PREFIX ${LIBCBOR_PREFIX}
        GIT_REPOSITORY https://github.com/PJK/libcbor.git
        GIT_TAG v0.5.0
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND mkdir -p ${LIBCBOR_PREFIX}/build &&
                          cd ${LIBCBOR_PREFIX}/build &&
                          cmake -DCMAKE_BUILD_TYPE=Release
                                -DCMAKE_INSTALL_PREFIX=${LIBCBOR_PREFIX}/install
                                -DCMAKE_INSTALL_LIBDIR=lib
                                -DCMAKE_C_FLAGS=-fPIC
                                ${LIBCBOR_PREFIX}/src/project_libcbor
        BUILD_COMMAND make -C ${LIBCBOR_PREFIX}/build -j8 cbor cbor_shared
        INSTALL_COMMAND make -C ${LIBCBOR_PREFIX}/build install
        BUILD_BYPRODUCTS ${LIBCBOR_PREFIX}/install/lib/libcbor.a

)

set(LIBCBOR_ROOT ${LIBCBOR_PREFIX}/install)
set(LIBCBOR_ROOT ${LIBCBOR_PREFIX}/install PARENT_SCOPE)
set(LIBCBOR_INCLUDE_DIRS ${LIBCBOR_PREFIX}/install/include)
set(LIBCBOR_LIB_DIR ${LIBCBOR_PREFIX}/install/lib/)

add_library(libcbor STATIC IMPORTED)
add_dependencies(libcbor project_libcbor)
set_property(TARGET libcbor PROPERTY IMPORTED_LOCATION ${LIBCBOR_PREFIX}/install/lib/libcbor.a)
