#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>

#include "base64.h"

namespace satori {
namespace video {

std::string decode64(const std::string &val) {
  using namespace boost::archive::iterators;
  using It = transform_width<binary_from_base64<std::string::const_iterator>, 8, 6>;
  auto decoded = std::string(It(std::begin(val)), It(std::end(val)));
  auto padding = val.find('=');
  if (padding == std::string::npos) {
    return decoded;
  }
  return decoded.substr(0, decoded.size() - padding);
}

std::string encode64(const std::string &val) {
  using namespace boost::archive::iterators;
  using It = base64_from_binary<transform_width<std::string::const_iterator, 6, 8>>;
  auto tmp = std::string(It(std::begin(val)), It(std::end(val)));
  return tmp.append((3 - val.size() % 3) % 3, '=');
}

}  // namespace video
}  // namespace satori
