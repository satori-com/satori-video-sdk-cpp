# Building Satori Video SDK

Supported platforms: Mac, Linux. [Install Prerequisites](prerequisites.md)

## Building

* Login to conan server: `make conan-login`
* `mkdir build && cd build && cmake ../ && make -j 8`

Running clang-tidy:
* `cmake -DCMAKE_CXX_CLANG_TIDY="/usr/local/opt/llvm/bin/clang-tidy"`
