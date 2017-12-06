# Video bot prerequisites

## Prerequisites
* Mac OS X or Linux
* C++14 compiler

# Software

## Mac OS X:
* Docker [Install Docker](https://docs.docker.com/engine/installation/)
* conan
* cmake
* autoconf
* libtool
* gnu make, version >=3.82

**Note:**
* Use `brew install`, except for gnu make
* For gnu make, use `brew install --with-default-names` so that `brew`
installs the tool as `make` rather than `gnu-make`.

## Linux:

* build-essential
* clang
* cmake
* git
* ninja-build
* python-pip
* yasm
* zlib1g-dev
* clang-tidy
* autoconf
* libtool
* curl
* conan, version >=0.28.0
* wget

**Note:**
* Use `sudo apt-get install -y`, except for conan
* For conan, use `sudo pip install 'conan>=0.28.0'`

## Conan

Specify the conan remote server:
```shell
$ conan remote update video-conan 'http://video-conan.api.satori.com'
```
