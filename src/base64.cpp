#include "base64.h"

#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>

#include "logging.h"

namespace it = boost::archive::iterators;

namespace satori {
namespace video {
namespace base64 {

streams::error_or<std::string> decode(const std::string &val) {
  using iterator_t =
      it::transform_width<it::binary_from_base64<std::string::const_iterator>, 8, 6>;

  std::string decoded;
  try {
    decoded = std::string{iterator_t{std::begin(val)}, iterator_t{std::end(val)}};
  } catch (const std::exception &e) {
    LOG(ERROR) << "input is not base64: " << e.what() << ", value: " << val;
    return std::system_category().default_error_condition(EBADMSG);
  }

  const auto padding = val.find('=');
  if (padding == std::string::npos) {
    return decoded;
  }
  return decoded.substr(0, decoded.size() - (val.size() - padding));
}

std::string encode(const std::string &val) {
  using iterator_t =
      it::base64_from_binary<it::transform_width<std::string::const_iterator, 6, 8>>;
  auto encoded = std::string{iterator_t{std::begin(val)}, iterator_t{std::end(val)}};
  return encoded.append((3 - val.size() % 3) % 3, '=');
}

}  // namespace base64
}  // namespace video
}  // namespace satori
