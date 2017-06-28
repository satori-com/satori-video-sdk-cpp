# - Try to find libcbor.

find_path(LIBCBOR_INCLUDE_DIR
        NAMES "cbor.h"
        PATHS
            "${LIBCBOR_ROOT}/include"
            "/usr/include"
            "/usr/local/include"
        )

find_library(LIBCBOR_LIBRARY
        NAMES "libcbor.a"
        PATHS
            "${LIBCBOR_ROOT}/lib"
            "/usr/lib/"
            "/usr/local/lib/"
        )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBRTMVIDEO
        REQUIRED_VARS LIBCBOR_INCLUDE_DIR LIBCBOR_LIBRARY)


set(LIBRTMVIDEO_INCLUDE_DIRS ${LIBCBOR_INCLUDE_DIR})
set(LIBRTMVIDEO_LIBRARIES ${LIBCBOR_LIBRARY})

mark_as_advanced(LIBRTMVIDEO_INCLUDE_DIRS LIBRTMVIDEO_LIBRARIES)