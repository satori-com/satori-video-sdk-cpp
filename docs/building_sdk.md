# Building Satori Video SDK

## Prerequisites
* Mac OS X or Linux
* C++14 compiler
* [Software Prerequisites](prerequisites.md)

## Building

`$ mkdir build && cd build && cmake ../ && make -j 8`

## Trying Local Changes

To try out local changes with a bot, export changes 
to local conan repository: `conan export satorivideo/master`

### Building with ASAN:
`$ cmake -DCMAKE_CXX_SANITIZER="address"`

### Building with clang-tidy:
`$ cmake -DCMAKE_CXX_CLANG_TIDY="/usr/local/opt/llvm/bin/clang-tidy"`
