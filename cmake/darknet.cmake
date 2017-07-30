# Deep learning neural network library
# need to be patched to work on MAC and work with OpenMP

set(DARKNET_PREFIX ${CMAKE_BINARY_DIR}/darknet)

if(UNIX AND NOT APPLE)
ExternalProject_Add(
        darknetproject
        PREFIX ${DARKNET_PREFIX}
        GIT_REPOSITORY https://github.com/pjreddie/darknet.git
        GIT_TAG 7a223d8591e0a497889b9fce9bc43ac4bd3969fd # this packet has no releases, it is known stable commit
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND bash -c "sed -i -e 's/OPENMP=0/OPENMP=1/g' ${DARKNET_PREFIX}/src/darknetproject/Makefile"
        BUILD_COMMAND make -C ${DARKNET_PREFIX}/src/darknetproject
        INSTALL_COMMAND ""
)
set(DARKNET_LIBS darknet PARENT_SCOPE)
endif()
if(APPLE)
ExternalProject_Add(
        darknetproject
        PREFIX ${DARKNET_PREFIX}
        GIT_REPOSITORY https://github.com/pjreddie/darknet.git
        GIT_TAG 7a223d8591e0a497889b9fce9bc43ac4bd3969fd # this packet has no releases, it is known stable commit
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND bash -c "sed -i -e 's/clock_gettime(CLOCK_REALTIME, &now)//g' ${DARKNET_PREFIX}/src/darknetproject/src/utils.c"
        BUILD_COMMAND make -C ${DARKNET_PREFIX}/src/darknetproject
        INSTALL_COMMAND ""
)
set(DARKNET_LIBS darknet PARENT_SCOPE)
endif()

set(DARKNET_INCLUDE_DIR ${DARKNET_PREFIX}/src/darknetproject/include/ PARENT_SCOPE)
set(DARKNET_LIB_DIR ${DARKNET_PREFIX}/src/darknetproject/ PARENT_SCOPE)
include_directories(${DARKNET_PREFIX}/src/darknetproject/include/)
