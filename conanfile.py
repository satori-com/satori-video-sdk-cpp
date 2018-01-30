from conans import ConanFile, CMake
import os


class SatorivideoConan(ConanFile):
    name = "SatoriVideo"
    url = "https://github.com/satori-com/satori-video-sdk-cpp"
    description = "Satori Video Client Library"

    options = {"with_opencv": [True, False], "with_gperftools": [True, False]}

    requires = "Boost/1.66.0-02@satorivideo/master", \
               "Ffmpeg/3.4.0@satorivideo/master", \
               "Gsl/20017.07.27@satorivideo/master", \
               "Json/3.0.1@satorivideo/master", \
               "Libcbor/0.5.0@satorivideo/master", \
               "Loguru/1.5.0@satorivideo/master", \
               "Openssl/1.1.0g@satorivideo/master", \
               "PrometheusCpp/2017.12.13@satorivideo/master", \
               "SDL/2.0.5@satorivideo/master"

    license = "proprietary"
    version = '0.15.10'
    settings = "os", "compiler", "build_type", "arch"
    default_options = "with_opencv=True", \
                      "with_gperftools=True", \
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
                      "PrometheusCpp:fPIC=True"

    generators = "cmake"
    exports_sources = "*", ".clang-tidy", ".clang-format", \
                      "!build*", "!cmake-build-*", "!Dockerfile", "!Makefile"

    def requirements(self):
        if self.options.with_opencv:
            self.requires("Opencv/3.4.0@satorivideo/master")
            self.options["Opencv"].shared = False
            self.options["Opencv"].fPIC = True
        if self.options.with_gperftools:
            self.requires("GPerfTools/2.6.3@satorivideo/master")
            self.options["GPerfTools"].shared = False
            self.options["GPerfTools"].fPIC = True

    def build(self):
        cmake = CMake(self)
        cmake_options = []
        cmake_options.append("-DCMAKE_VERBOSE_MAKEFILE=ON")

        if "CMAKE_CXX_CLANG_TIDY" in os.environ:
            cmake_options.append("-DCMAKE_CXX_CLANG_TIDY='%s'" %
                                 os.environ["CMAKE_CXX_CLANG_TIDY"])

        cmake_generate_command = ('cmake . %s %s' %
                                  (cmake.command_line, " ".join(cmake_options)))
        self.output.info("running cmake: %s" % cmake_generate_command)
        self.run(cmake_generate_command)

        cmake_build_command = "VERBOSE=1 cmake --build . %s -- -j 8" % cmake.build_config
        self.output.info("running build: %s" % cmake_build_command)
        self.run(cmake_build_command)
        self.output.info("running tests")
        self.run("CTEST_OUTPUT_ON_FAILURE=ON ctest -j 8 .")

    def package(self):
        excludes = None
        if not self.options.with_opencv:
            excludes = "opencv"

        # include
        self.copy("*.h", dst="include", src="include", excludes=excludes)
        self.copy("*.h", dst="include/satorivideo/impl",
                  src="src", excludes=excludes)

        # lib
        self.copy("*.lib", dst="lib", keep_path=False)
        self.copy("*.so", dst="lib", keep_path=False)
        self.copy("*.dylib", dst="lib", keep_path=False)
        self.copy("*.a", dst="lib", keep_path=False)

        # bin
        self.copy("*", dst="bin", src="bin", keep_path=False)
        self.copy("*.dll", dst="bin", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["satorivideo"]
        self.env_info.path.append(os.path.join(self.package_folder, "bin"))

        # https://gcc.gnu.org/onlinedocs/gcc/Link-Options.html
        # This option is needed for some uses of dlopen or to allow obtaining
        # backtraces from within a program
        if self.settings.compiler == "gcc":
            self.cpp_info.cflags = ["-rdynamic"]
            self.cpp_info.cppflags = ["-rdynamic"]
