# Building Satori Video SDK

Supported platforms: Mac & Linux with C++14 compiler. [Install Prerequisites](prerequisites.md)

## Building

\* `mkdir build && cd build && cmake ../ && make -j 8`

## Trying Local Changes

To try out local changes with a bot, export changes 
to local conan repository: `conan export satorivideo/master`

### Building with ASAN:
* `cmake -DCMAKE_CXX_SANITIZER="address"`

### Building with clang-tidy:
* `cmake -DCMAKE_CXX_CLANG_TIDY="/usr/local/opt/llvm/bin/clang-tidy"`
