from conans import ConanFile, CMake


class SatorivideoConan(ConanFile):
    name = "SatoriVideo"
    url = "https://bitbucket.addsrv.com/projects/PLATFORM/repos/video/browse"
    description = "Satori Video Client Library"

    options = {"with_opencv": [True, False], "sanitizer": ["", "address"]}


    requires = "Libcbor/0.5.0@satorivideo/master", \
               "Boost/1.65.1@satorivideo/master", \
               "Openssl/1.1.0f@satorivideo/master", \
               "Beast/123@satorivideo/master", \
               "Gsl/20017.07.27@satorivideo/master", \
               "Rapidjson/1.1.0@satorivideo/master", \
               "Ffmpeg/3.3.3_07@satorivideo/master", \
               "Loguru/1.5.0@satorivideo/master", \
               "SDL/2.0.5@satorivideo/master", \
               "GPerfTools/2017.10.16@satorivideo/master", \
               "PrometheusCpp/2017.09.28@satorivideo/master"

    license = "proprietary"
    version = '0.8.0'
    settings = "os", "compiler", "build_type", "arch"
    default_options = "with_opencv=True", \
                      "sanitizer=", \
                      "Libcbor:fPIC=True", \
                      "Libcbor:shared=False", \
                      "Boost:fPIC=True", \
                      "Boost:shared=False", \
                      "Openssl:fPIC=True", \
                      "Openssl:shared=False", \
                      "Ffmpeg:shared=False", \
                      "Ffmpeg:fPIC=True", \
                      "SDL:shared=False", \
                      "SDL:fPIC=True", \
                      "PrometheusCpp:shared=False", \
                      "PrometheusCpp:fPIC=True", \
                      "GPerfTools:shared=False", \
                      "GPerfTools:fPIC=True"

    generators = "cmake"
    exports_sources = "*"

    def requirements(self):
        if self.options.with_opencv:
            self.requires("Opencv/3.3.0_02@satorivideo/master")
            self.options["Opencv"].shared = False
            self.options["Opencv"].fPIC = True

    def build(self):
        cmake = CMake(self)
        if self.options.sanitizer:
            cmake.definitions["CMAKE_CXX_SANITIZER"] = self.options.sanitizer

        cmake_options = []
        cmake_options.append("-DCMAKE_VERBOSE_MAKEFILE=ON")

        cmake_generate_command = ('cmake . %s %s' %
                                  (cmake.command_line, " ".join(cmake_options)))
        self.output.info(cmake_generate_command)
        self.run(cmake_generate_command)

        cmake_build_command = "VERBOSE=1 cmake --build . %s -- -j 8" % cmake.build_config
        self.output.info(cmake_build_command)
        self.run(cmake_build_command)
        self.run("CTEST_OUTPUT_ON_FAILURE=ON ctest -V -j 8 .")

    def package(self):
        excludes = None
        if not self.options.with_opencv:
            excludes = "opencv"

        self.copy("*.h", dst="include", src="include", excludes=excludes)
        self.copy("*.h", dst="include/satorivideo/impl", src="src", excludes=excludes)
        self.copy("*.lib", dst="lib", keep_path=False)
        self.copy("*.dll", dst="bin", keep_path=False)
        self.copy("*.so", dst="lib", keep_path=False)
        self.copy("*.dylib", dst="lib", keep_path=False)
        self.copy("*.a", dst="lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["satorivideo"]
