.RECIPEPREFIX = >

.PHONY: conan-upload

OS := $(shell uname)

conan-create:
> @if [ ${OS} == Darwin ] ; then \
>   echo "Using compiler.version=8.1" ; \
> 	conan create satorivideo/master --build=missing -s compiler.version=8.1 ; \
> else \
> 	conan create satorivideo/master --build=missing ; \
> fi ;

conan-upload: CONAN_PACKAGE ?= *
conan-upload: conan-create
> @if [ -z ${CONAN_USER} ] ; then echo "CONAN_USER not set" ; exit 1 ; fi ;
> @if [ -z ${CONAN_PASSWORD} ] ; then echo "CONAN_PASSWORD not set" ; exit 1 ; fi ;
> @echo "Will upload ${CONAN_PACKAGE}"
> conan remote add video http://10.199.28.20:80/; conan user --remote video -p ${CONAN_PASSWORD} ${CONAN_USER}
> conan upload --confirm --remote video --all '${CONAN_PACKAGE}@satorivideo/master' && echo 'SUCCESS'
