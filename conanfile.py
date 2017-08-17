from conans import ConanFile, CMake
import os


class SatorivideoConan(ConanFile):
    name = "SatoriVideo"
    url = "https://bitbucket.addsrv.com/projects/PLATFORM/repos/video/browse"
    description = "Satori Video Client Library"
    requires = "Libcbor/0.5.0@satorivideo/master", \
               "Boost/1.64.0@satorivideo/master", \
               "Openssl/1.1.0f@satorivideo/master", \
               "Beast/95@satorivideo/master", \
               "Gsl/20017.07.27@satorivideo/master", \
               "Rapidjson/1.1.0@satorivideo/master", \
               "Ffmpeg/3.3.3@satorivideo/master"
    license = "proprietary"
    version = '0.1.8'
    settings = "os", "compiler", "build_type", "arch"
    default_options = "Libcbor:fPIC=True", \
                      "Libcbor:shared=False", \
                      "Boost:fPIC=True", \
                      "Boost:shared=False", \
                      "Openssl:fPIC=True", \
                      "Openssl:shared=False", \
                      "Ffmpeg:shared=False", \
                      "Ffmpeg:fPIC=True"

    generators = "cmake"
    exports_sources = "*"

    def build(self):
        cmake = CMake(self)
        self.run('cmake . %s' % cmake.command_line )
        self.run("cmake --build . %s" % cmake.build_config)

    def package(self):
        self.copy("*.h", dst="include", src="include")
        self.copy("*.lib", dst="lib", keep_path=False)
        self.copy("*.dll", dst="bin", keep_path=False)
        self.copy("*.so", dst="lib", keep_path=False)
        self.copy("*.dylib", dst="lib", keep_path=False)
        self.copy("*.a", dst="lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["rtmvideo", "rtmvideo-decoder"]
