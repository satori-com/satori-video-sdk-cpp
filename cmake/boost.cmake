include(ExternalProject)

set(BoostPrefix ${CMAKE_BINARY_DIR}/boost)
set(BOOST_ROOT_DIR ${BoostPrefix})

set(BOOST_LIB_DIR ${BOOST_ROOT_DIR}/src/boost/stage/lib)

ExternalProject_Add(
        boost
        URL https://dl.bintray.com/boostorg/release/1.64.0/source/boost_1_64_0.tar.gz
        URL_HASH SHA256=0445c22a5ef3bd69f5dfb48354978421a85ab395254a26b1ffb0aa1bfd63a108
        PREFIX ${BoostPrefix}
        CONFIGURE_COMMAND bash -c "./bootstrap.sh --prefix=<INSTALL_DIR> --with-libraries=system,regex,program_options"
        BUILD_COMMAND bash -c "./b2 --build-dir=<TMP_DIR>"
        INSTALL_COMMAND bash -c "./b2 --install --build-dir=<TMP_DIR>"
        BUILD_IN_SOURCE 1
        BUILD_BYPRODUCTS 
                ${BOOST_LIB_DIR}/libboost_system.a
                ${BOOST_LIB_DIR}/libboost_regex.a
                ${BOOST_LIB_DIR}/libboost_program_options.a
)

include_directories(${BOOST_ROOT_DIR}/src/boost/)

add_library(boost_system STATIC IMPORTED)
add_dependencies(boost_system boost)
set_property(TARGET boost_system PROPERTY IMPORTED_LOCATION ${BOOST_LIB_DIR}/libboost_system.a)

add_library(boost_regex STATIC IMPORTED)
add_dependencies(boost_regex boost)
set_property(TARGET boost_regex PROPERTY IMPORTED_LOCATION ${BOOST_LIB_DIR}/libboost_regex.a)

add_library(boost_program_options STATIC IMPORTED)
add_dependencies(boost_program_options boost)
set_property(TARGET boost_program_options PROPERTY IMPORTED_LOCATION ${BOOST_LIB_DIR}/libboost_program_options.a)