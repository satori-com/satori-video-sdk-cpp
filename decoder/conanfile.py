from conans import ConanFile, CMake


class DecoderConan(ConanFile):
    name = "Decoder"
    url = "https://bitbucket.addsrv.com/projects/PLATFORM/repos/video/browse"
    description = "Satori Video Client Library"
    requires = "Ffmpeg/3.3.3_02@satorivideo/master"
    license = "proprietary"
    version = "0.1"
    settings = "os", "compiler", "build_type", "arch"
    options = {"emcc": [True, False]}
    default_options = "emcc=False"

    generators = "cmake"
    exports_sources = "*"

    def requirements(self):
        self.options["Ffmpeg"].emcc = self.options.emcc
        self.options["Ffmpeg"].shared = self.options.emcc
        self.options["Ffmpeg"].fPIC = True

    def build(self):
        cmake = CMake(self)
        self.run('cmake . %s -DCMAKE_VERBOSE_MAKEFILE=ON' %
                 (cmake.command_line))
        self.run("cmake --build . %s" % cmake.build_config)

    def package(self):
        self.copy("*.h", dst="include", src="librtmvideo/include")
        self.copy("*.lib", dst="lib", keep_path=False)
        self.copy("*.dll", dst="bin", keep_path=False)
        self.copy("*.so", dst="lib", keep_path=False)
        self.copy("*.dylib", dst="lib", keep_path=False)
        self.copy("*.a", dst="lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["rtmvideo-decoder"]
