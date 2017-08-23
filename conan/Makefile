CONAN_SERVER?=http://10.199.28.20:80/
CONAN_REMOTE?=video
CONAN_USER?=video
CONAN_PASSWORD?=video

.RECIPEPREFIX = >
.PHONY: conan-login

conan-login:
> conan remote remove ${CONAN_REMOTE} ; conan remote add ${CONAN_REMOTE} ${CONAN_SERVER}
> conan user --remote ${CONAN_REMOTE} -p ${CONAN_PASSWORD} ${CONAN_USER}
