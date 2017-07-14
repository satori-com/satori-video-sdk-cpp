.RECIPEPREFIX = >

IMAGE_NAME = gcr.io/kubernetes-live/video/verifier-bot
BASE_IMAGE_API_TAG = api-1.0
IMAGE_API_TAG = ${BASE_IMAGE_API_TAG}-verifier-bot

all: push

cpp-build:
> $(MAKE) -C ../../ cpp-build

in/libs.tgz: cpp-build
> mkdir -p in
> docker run --rm cpp-build bash -c "find /out -type f \( -name "*.a" -o -name "*.so" -o -name "*.so.*" \) -print0 | tar -czhf - --null -T -" > $@

in/verifier-bot: cpp-build
> mkdir -p in
> docker run --rm cpp-build cat /out/bots/verifier-bot/verifier-bot > $@

build: in/libs.tgz
build: in/verifier-bot
> @echo "Building bot image"
> docker build --no-cache=true -t local-newbot-verifier .

check: build
> @echo "Running image verifier bot"
> docker run --entrypoint=/app/test.sh --rm local-newbot-verifier

push: check
> @echo "Pushing video bot image"
> docker push ${IMAGE_NAME}
