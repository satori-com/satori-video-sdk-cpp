CONAN_SERVER?=
CONAN_USER?=
CONAN_PASSWORD?=
CONAN_REMOTE?=video
CONAN_UPLOAD_OPTIONS?=--all

DOCKER_BUILD_OPTIONS?=
BUILD_TYPE=RelWithDebInfo
.RECIPEPREFIX = >


.PHONY: all conan-create conan-create-in-docker conan-upload-from-docker conan-login conan-upload

CONAN_CREATE_ARGS ?= --build=outdated

DOCKER_TAG=satorivideo

all: conan-create-in-docker

conan-create:
> conan create satorivideo/master ${CONAN_CREATE_ARGS}

conan-create-in-docker:
> docker build ${DOCKER_BUILD_OPTIONS} --build-arg CONAN_CREATE_ARGS="--build=outdated -s compiler.libcxx=libstdc++11 -s build_type=${BUILD_TYPE}" -t ${DOCKER_TAG} .

conan-upload-from-docker:
> docker run --rm ${DOCKER_TAG} bash -c "CONAN_REMOTE=${CONAN_REMOTE} CONAN_SERVER=${CONAN_SERVER} CONAN_USER=${CONAN_USER} CONAN_PASSWORD=${CONAN_PASSWORD} CONAN_UPLOAD_OPTIONS='${CONAN_UPLOAD_OPTIONS}' make conan-login conan-upload"

build-clang:
> docker build ${DOCKER_BUILD_OPTIONS} --build-arg CONAN_CREATE_ARGS="-p clang --build=outdated -s build_type=${BUILD_TYPE}" -t ${DOCKER_TAG}-clang .

build-asan:
> docker build ${DOCKER_BUILD_OPTIONS} --build-arg CONAN_CREATE_ARGS="-p asan --build=outdated -s build_type=${BUILD_TYPE}" -t ${DOCKER_TAG}-asan .

build-tidy:
> docker build ${DOCKER_BUILD_OPTIONS} --build-arg CONAN_CREATE_ARGS="-p tidy --build=outdated -s build_type=${BUILD_TYPE}" -t ${DOCKER_TAG}-tidy .

## FIXME: had to duplicate it for now
conan-login:
> conan remote remove ${CONAN_REMOTE} ; conan remote add ${CONAN_REMOTE} ${CONAN_SERVER}
> conan user --remote ${CONAN_REMOTE} -p ${CONAN_PASSWORD} ${CONAN_USER}

## FIXME: had to duplicate it for now
conan-upload:
> conan upload --confirm ${CONAN_UPLOAD_OPTIONS} --remote ${CONAN_REMOTE} '*@satorivideo/*'
> conan remove -r ${CONAN_REMOTE} -f --outdated '*@satorivideo/*'
