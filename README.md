# Satori Video C++ SDK

## Building SDK

### Prerequisites

* Install Docker: [https://docs.docker.com/engine/installation/](https://docs.docker.com/engine/installation/)

* Conan, cmake, gnu make >= 3.82, autoconf, libtool:
  - (Mac) `brew install conan cmake autoconf libtool && brew install make --with-default-names`
  - (Linux) `sudo apt-get install -y build-essential clang cmake git ninja-build python-pip yasm zlib1g-dev clang-tidy autoconf libtool curl wget && sudo pip install 'conan>=0.28.0'`

* Define `CONAN_SERVER`, `CONAN_REMOTE`, `CONAN_USER`, `CONAN_PASSWORD`, environment variables with access credentials to satori video conan server. (Shell plugins like autoenv are recommended)

### Building

* Login to conan server: `make conan-login`
* `mkdir build && cd build && cmake ../ && make -j 8`

Running clang-tidy:
* `cmake -DCMAKE_CXX_CLANG_TIDY="/usr/local/opt/llvm/bin/clang-tidy"`
