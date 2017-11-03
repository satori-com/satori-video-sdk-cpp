FROM gcr.io/kubernetes-live/video/cpp-builder

ARG BUILD_TYPE=Release

ARG CONAN_REMOTE=video
ARG CONAN_SERVER=http://10.199.28.20:80/
ARG CONAN_USER=video
ARG CONAN_PASSWORD=video
ARG CONAN_CREATE_ARGS="--build=outdated"

RUN mkdir /src
COPY . /src

WORKDIR /src
RUN make conan-login conan-create
