set(FFMPEG_PREFIX ${CMAKE_BINARY_DIR}/ffmpeg)

set(FFMPEG_CONFIGURE_ARGS
        --prefix=${FFMPEG_PREFIX}
        "--extra-ldflags=-L${LIBVPX_LIB_DIR} -lvpx" --extra-cflags=-I${LIBVPX_INCLUDE_DIR}
        --disable-programs --disable-everything
        --enable-libvpx
        --enable-decoder=h264 --enable-decoder=mjpeg
        --enable-decoder=libvpx_vp8 --enable-decoder=libvpx_vp9
        --enable-encoder=libvpx_vp8 --enable-encoder=libvpx_vp9
        --enable-pic
    )

IF(CMAKE_BUILD_TYPE MATCHES Debug)
    message("*** Building Debug Version of ffmpeg")
    set(FFMPEG_CONFIGURE_ARGS ${FFMPEG_CONFIGURE_ARGS}
            --disable-optimizations --disable-mmx --disable-stripping --enable-debug=3)
ENDIF()

ExternalProject_Add(
        project_ffmpeg
        PREFIX ${FFMPEG_PREFIX}
        GIT_REPOSITORY https://github.com/FFmpeg/FFmpeg.git
        GIT_TAG n3.3
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND
            ${FFMPEG_PREFIX}/src/project_ffmpeg/configure ${FFMPEG_CONFIGURE_ARGS}
        BUILD_COMMAND make -j8
        INSTALL_COMMAND make install
        BUILD_BYPRODUCTS 
            ${FFMPEG_PREFIX}/lib/libavcodec.a
            ${FFMPEG_PREFIX}/lib/libavutil.a
            ${FFMPEG_PREFIX}/lib/libavdevice.a
            ${FFMPEG_PREFIX}/lib/libavformat.a
            ${FFMPEG_PREFIX}/lib/libswscale.a            
)

include_directories(${FFMPEG_PREFIX}/include/)

add_library(avcodec STATIC IMPORTED)
add_dependencies(avcodec project_ffmpeg)
set_property(TARGET avcodec PROPERTY IMPORTED_LOCATION ${FFMPEG_PREFIX}/lib/libavcodec.a)

add_library(avutil STATIC IMPORTED)
add_dependencies(avutil project_ffmpeg)
set_property(TARGET avutil PROPERTY IMPORTED_LOCATION ${FFMPEG_PREFIX}/lib/libavutil.a)

add_library(avdevice STATIC IMPORTED)
add_dependencies(avdevice project_ffmpeg)
set_property(TARGET avdevice PROPERTY IMPORTED_LOCATION ${FFMPEG_PREFIX}/lib/libavdevice.a)

add_library(avformat STATIC IMPORTED)
add_dependencies(avformat project_ffmpeg)
set_property(TARGET avformat PROPERTY IMPORTED_LOCATION ${FFMPEG_PREFIX}/lib/libavformat.a)

add_library(swscale STATIC IMPORTED)
add_dependencies(swscale project_ffmpeg)
set_property(TARGET swscale PROPERTY IMPORTED_LOCATION ${FFMPEG_PREFIX}/lib/libswscale.a)
