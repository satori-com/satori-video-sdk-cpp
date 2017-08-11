# for MAC use -s compiler.version=8.1 to fix compiler version detected by conan
conan create satorivideo/master --build=missing
conan remote add video http://10.199.28.20:80/; conan user --remote video -p ${CONAN_PASSWORD} ${CONAN_USER}
conan upload --confirm --remote video --all '*@satorivideo/*' && echo 'SUCCESS'
