include(ExternalProject)
set(OPENSSL_PREFIX ${CMAKE_BINARY_DIR}/openssl)

ExternalProject_Add(
        openssl
        PREFIX ${OPENSSL_PREFIX}
        URL https://www.openssl.org/source/openssl-1.1.0e.tar.gz
        URL_HASH SHA256=57be8618979d80c910728cfc99369bf97b2a1abd8f366ab6ebdee8975ad3874c
        CONFIGURE_COMMAND KERNEL_BITS=64 ${OPENSSL_PREFIX}/src/openssl/config no-shared --prefix=${OPENSSL_PREFIX} --openssldir=${OPENSSL_PREFIX}
        BUILD_COMMAND make depend -j8 && make -j8
        INSTALL_COMMAND make install_sw
        BUILD_BYPRODUCTS ${OPENSSL_PREFIX}/lib/libcrypto.a ${OPENSSL_PREFIX}/lib/libssl.a
)

include_directories(${OPENSSL_PREFIX}/include)

add_library(crypto STATIC IMPORTED)
add_dependencies(crypto openssl)
set_property(TARGET crypto PROPERTY IMPORTED_LOCATION ${OPENSSL_PREFIX}/lib/libcrypto.a)

add_library(ssl STATIC IMPORTED)
add_dependencies(ssl openssl)
set_property(TARGET ssl PROPERTY IMPORTED_LOCATION ${OPENSSL_PREFIX}/lib/libssl.a)