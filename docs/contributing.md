# Contributing to the Satori Video SDK for C++ project

[All Video SDK documentation](../README.md)

## Prerequisites
* macOS X or Linux
* `gcc` or `clang` compiler
* [Other prerequisites](prerequisites.md)

## Clone the SDK
```shell
$ git clone https://github.com/satori-com/satori-video-sdk-cpp.git
```

## Build the SDK
**Note:** The first build requires more time than subsequent builds, because the `conan` package manager
needs to retrieve and build dependencies.

```shell
$ cd satori-video-sdk-cpp
$ mkdir build 
$ cd build 
$ cmake ../
$ make -j 8
```

## Test SDK modifications

Test your changes by exporting them to your local conan cache:
```shell
$conan export satorivideo/master
```

### Build with ASAN memory error detection:
`$ cmake -DCMAKE_CXX_SANITIZER="address"`

### Build with `clang-tidy`:
`$ cmake -DCMAKE_CXX_CLANG_TIDY="/usr/local/opt/llvm/bin/clang-tidy"`
