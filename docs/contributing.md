# Contributing to the Satori Video SDK for C++ project

[All Video SDK documentation](../README.md)

## Table of contents
* [Prerequisites](#prerequisites)
* [Clone the SDK](#clone-the-sdk)
* [Build the SDK](#build-the-sdk)
* [Test SDK modifications](#test-sdk-modifications)

## Prerequisites
See [Satori Video SDK for C++ Prerequisites](prerequisites.md).

## Clone the SDK
```shell
$ git clone https://github.com/satori-com/satori-video-sdk-cpp.git
```

## Build the SDK
The first build requires more time than subsequent builds, because the `conan` package manager
needs to retrieve and build dependencies.

If you encounter errors during the build, try removing the `build` directory and re-building.

```shell
$ cd satori-video-sdk-cpp
$ mkdir build 
$ cd build 
$ cmake ../
$ make -j 8
```

## Test SDK modifications

Test your changes by exporting them from the root of `satori-video-sdk-cpp` to your local conan cache:
```shell
$conan export <video-sdk-root> satorivideo/master
```

### Build with ASAN memory error detection:
`$ cmake -DCMAKE_CXX_SANITIZER="address"`

### Build with `clang-tidy`:
`$ cmake -DCMAKE_CXX_CLANG_TIDY="/usr/local/opt/llvm/bin/clang-tidy"`
