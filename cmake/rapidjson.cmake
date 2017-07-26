include(ExternalProject)
set(RAPIDJSON_PREFIX ${CMAKE_BINARY_DIR}/rapidjson)

ExternalProject_Add(
        rapidjson
        PREFIX ${RAPIDJSON_PREFIX}
        GIT_REPOSITORY https://github.com/miloyip/rapidjson.git
        GIT_TAG v1.1.0
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND "" # skip configure
        BUILD_COMMAND "" # skip build
        INSTALL_COMMAND "" # skip install
)

include_directories(${RAPIDJSON_PREFIX}/src/rapidjson/include)
