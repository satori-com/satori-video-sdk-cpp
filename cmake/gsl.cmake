# C++ Core Guidelines Support Library
#
# API Documentation:
# - span: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0122r3.pdf

set(GSL_PREFIX ${CMAKE_BINARY_DIR}/gsl)

ExternalProject_Add(
        gsl
        PREFIX ${GSL_PREFIX}
        GIT_REPOSITORY https://github.com/Microsoft/GSL.git
        GIT_TAG 1f87ef73f1477e8adafa8b10ccee042897612a20
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
)

include_directories(${GSL_PREFIX}/src/gsl/include/)
set(GSL_INCLUDE_DIR ${GSL_PREFIX}/src/gsl/include/)
set(GSL_INCLUDE_DIR ${GSL_PREFIX}/src/gsl/include/ PARENT_SCOPE)
