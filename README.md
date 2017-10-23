# Satori Video C++ SDK

## Building SDK

### Prerequisites

* Install Docker: [https://docs.docker.com/engine/installation/](https://docs.docker.com/engine/installation/)

* Conan, cmake, gnu make >= 3.82, autoconf:
  - (Mac) `brew install conan cmake autoconf && brew install make --with-default-names`
  - (Linux) :wrench: _TODO_

* Define `CONAN_SERVER`, `CONAN_REMOTE`, `CONAN_USER`, `CONAN_PASSWORD`, environment variables with access credentials to satori video conan server. (Shell plugins like autoenv are recommended)

### Building

* Login to conan server: `make conan-login`
* `mkdir build && cd build && cmake ../ && make -j 8`
