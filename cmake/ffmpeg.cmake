set(FFMPEG_PREFIX ${CMAKE_BINARY_DIR}/ffmpeg)

set(FFMPEG_CONFIGURE_ARGS "")

IF(CMAKE_BUILD_TYPE MATCHES Debug)
    message("*** Building Debug Version of ffmpeg")
    set(FFMPEG_CONFIGURE_ARGS --disable-optimizations --disable-mmx --disable-stripping --enable-debug=3)
ENDIF()

ExternalProject_Add(
        ffmpeg
        PREFIX ${FFMPEG_PREFIX}
        GIT_REPOSITORY https://github.com/FFmpeg/FFmpeg.git
        GIT_TAG n3.3
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ${FFMPEG_PREFIX}/src/ffmpeg/configure --prefix=${FFMPEG_PREFIX}
                          --disable-programs --disable-everything
                          --enable-decoder=h264 --enable-decoder=mjpeg
                          --enable-pic
                          ${FFMPEG_CONFIGURE_ARGS}
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
add_dependencies(avcodec ffmpeg)
set_property(TARGET avcodec PROPERTY IMPORTED_LOCATION ${FFMPEG_PREFIX}/lib/libavcodec.a)

add_library(avutil STATIC IMPORTED)
add_dependencies(avutil ffmpeg)
set_property(TARGET avutil PROPERTY IMPORTED_LOCATION ${FFMPEG_PREFIX}/lib/libavutil.a)

add_library(avdevice STATIC IMPORTED)
add_dependencies(avdevice ffmpeg)
set_property(TARGET avdevice PROPERTY IMPORTED_LOCATION ${FFMPEG_PREFIX}/lib/libavdevice.a)

add_library(avformat STATIC IMPORTED)
add_dependencies(avformat ffmpeg)
set_property(TARGET avformat PROPERTY IMPORTED_LOCATION ${FFMPEG_PREFIX}/lib/libavformat.a)

add_library(swscale STATIC IMPORTED)
add_dependencies(swscale ffmpeg)
set_property(TARGET swscale PROPERTY IMPORTED_LOCATION ${FFMPEG_PREFIX}/lib/libswscale.a)
