create satorivideo/master --build=missing
conan remote add video http://10.199.28.20:80/; conan user --remote video -p ${CONAN_PASSWORD} ${CONAN_USER}
conan upload --confirm --remote video --all '*@satorivideo/*' && echo 'SUCCESS'