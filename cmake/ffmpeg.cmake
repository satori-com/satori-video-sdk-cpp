include(ExternalProject)

set(FFMPEG_PREFIX ${CMAKE_BINARY_DIR}/ffmpeg)

set(FFMPEG_CONFIGURE_COMMAND ${FFMPEG_PREFIX}/src/project_ffmpeg/configure)
set(FFMPEG_MAKE_COMMAND make)
set(FFMPEG_CONFIGURE_ARGS
        --cc=${CMAKE_C_COMPILER} --cxx=${CMAKE_CXX_COMPILER}
        --prefix=${FFMPEG_PREFIX}
        --enable-pic
        "--extra-ldflags=-L${LIBVPX_LIB_DIR} -lvpx" --extra-cflags=-I${LIBVPX_INCLUDE_DIR}
        --disable-all --disable-programs --disable-everything --disable-sdl2
        --enable-avcodec --enable-avdevice --enable-avformat --enable-avutil --enable-swscale

        --enable-libvpx
        --enable-decoder=h264 --enable-decoder=mjpeg
        --enable-decoder=libvpx_vp9 --enable-encoder=libvpx_vp9
        --enable-decoder=rawvideo
        --enable-demuxer=mov
        --enable-encoder=jpeg2000 --enable-encoder=mjpeg
        --enable-protocol=file
        --enable-indev=avfoundation
    )
    
set(FFMPEG_PATCH_COMMAND )
set(FFMPEG_LIB_SUFFIX a)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    message("    ffmpeg: Emscripten")
    set(FFMPEG_CONFIGURE_COMMAND emconfigure ${FFMPEG_CONFIGURE_COMMAND})
    set(FFMPEG_MAKE_COMMAND emmake ${FFMPEG_MAKE_COMMAND})
    set(FFMPEG_CONFIGURE_ARGS ${FFMPEG_CONFIGURE_ARGS}
            --enable-cross-compile --target-os=none --arch=x86  --disable-asm --disable-runtime-cpudetect
            --disable-fast-unaligned --disable-pthreads
            --disable-static --enable-shared --disable-stripping
        )
    set(FFMPEG_PATCH_COMMAND patch -p1 < ${CMAKE_SOURCE_DIR}/../cmake/ffmpeg-js.patch)
    set(FFMPEG_LIB_SUFFIX so)
ENDIF()

IF(CMAKE_BUILD_TYPE MATCHES Debug)
    message("    ffmpeg: Debug")
    set(FFMPEG_CONFIGURE_ARGS ${FFMPEG_CONFIGURE_ARGS}
            --disable-optimizations --disable-mmx --disable-stripping --enable-debug=3)
ENDIF()

ExternalProject_Add(
        project_ffmpeg
        PREFIX ${FFMPEG_PREFIX}
        GIT_REPOSITORY https://github.com/FFmpeg/FFmpeg.git
        GIT_TAG n3.3.1
        UPDATE_COMMAND ""
        PATCH_COMMAND ${FFMPEG_PATCH_COMMAND}
        CONFIGURE_COMMAND ${FFMPEG_CONFIGURE_COMMAND} ${FFMPEG_CONFIGURE_ARGS}
        BUILD_COMMAND V=1 ${FFMPEG_MAKE_COMMAND} -j8
        INSTALL_COMMAND ${FFMPEG_MAKE_COMMAND} install
        BUILD_BYPRODUCTS 
            ${FFMPEG_PREFIX}/lib/libavcodec.${FFMPEG_LIB_SUFFIX}
            ${FFMPEG_PREFIX}/lib/libavutil.${FFMPEG_LIB_SUFFIX}
            ${FFMPEG_PREFIX}/lib/libavdevice.${FFMPEG_LIB_SUFFIX}
            ${FFMPEG_PREFIX}/lib/libavformat.${FFMPEG_LIB_SUFFIX}
            ${FFMPEG_PREFIX}/lib/libswscale.${FFMPEG_LIB_SUFFIX}
)

set(FFMPEG_FOUND 1)
set(FFMPEG_LIB_DIR ${FFMPEG_PREFIX}/lib/)
set(FFMPEG_INCLUDE_DIR ${FFMPEG_PREFIX}/include/)

add_library(avcodec STATIC IMPORTED)
add_dependencies(avcodec project_ffmpeg)
set_property(TARGET avcodec PROPERTY IMPORTED_LOCATION ${FFMPEG_PREFIX}/lib/libavcodec.${FFMPEG_LIB_SUFFIX})

add_library(avutil STATIC IMPORTED)
add_dependencies(avutil project_ffmpeg)
set_property(TARGET avutil PROPERTY IMPORTED_LOCATION ${FFMPEG_PREFIX}/lib/libavutil.${FFMPEG_LIB_SUFFIX})

add_library(avdevice STATIC IMPORTED)
add_dependencies(avdevice project_ffmpeg)
set_property(TARGET avdevice PROPERTY IMPORTED_LOCATION ${FFMPEG_PREFIX}/lib/libavdevice.${FFMPEG_LIB_SUFFIX})

add_library(avformat STATIC IMPORTED)
add_dependencies(avformat project_ffmpeg)
set_property(TARGET avformat PROPERTY IMPORTED_LOCATION ${FFMPEG_PREFIX}/lib/libavformat.${FFMPEG_LIB_SUFFIX})

add_library(swscale STATIC IMPORTED)
add_dependencies(swscale project_ffmpeg)
set_property(TARGET swscale PROPERTY IMPORTED_LOCATION ${FFMPEG_PREFIX}/lib/libswscale.${FFMPEG_LIB_SUFFIX})
