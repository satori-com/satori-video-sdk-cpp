#include "version.h"

namespace satori {
namespace video {

namespace {
// all these are defined during build
constexpr char compiled_time[] = CMAKE_GEN_TIME;
constexpr char conan_package_version[] = CONAN_PACKAGE_VERSION;
constexpr char conan_package_name[] = CONAN_PACKAGE_NAME;
constexpr char conan_arch[] = CONAN_SETTINGS_ARCH;
constexpr char conan_build_type[] = CONAN_SETTINGS_BUILD_TYPE;
constexpr char conan_compiler[] = CONAN_SETTINGS_COMPILER;
constexpr char conan_compiler_libcxx[] = CONAN_SETTINGS_COMPILER_LIBCXX;
constexpr char conan_compiler_version[] = CONAN_SETTINGS_COMPILER_VERSION;
constexpr char conan_os[] = CONAN_SETTINGS_OS;
constexpr char git_commit_hash[] = GIT_COMMIT_HASH;
}  // namespace

void log_library_version(loguru::Verbosity verbosity) {
  VLOG(verbosity) << conan_package_name << "/" << conan_package_version
                  << " " << conan_build_type << " (compiled " << compiled_time
                  << " " << conan_os << " " << conan_arch << " " << conan_compiler
                  << " " << conan_compiler_version << " " << conan_compiler_libcxx << ")";
}

}  // namespace video
}  // namespace satori
