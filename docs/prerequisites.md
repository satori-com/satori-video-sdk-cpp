# Prerequisites

Operating Systems: macOS or Linux.

C++14 compiler.

* Conan, cmake, gnu make >= 3.82, autoconf, libtool:
  - (Mac) `brew install conan cmake autoconf libtool && brew install make --with-default-names`
  - (Linux) `sudo apt-get install -y build-essential clang cmake git ninja-build python-pip yasm zlib1g-dev clang-tidy autoconf libtool curl wget && sudo pip install 'conan>=0.28.0'`

* Define conan remote server: `conan remote add video-conan "http://video-conan.api.satori.com"`
