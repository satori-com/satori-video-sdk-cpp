#pragma once

#include <json.hpp>
#include <ostream>

#include "streams/streams.h"

namespace satori {
namespace video {
namespace streams {

streams::observer<nlohmann::json> &ostream_sink(std::ostream &out);
}
}  // namespace video
}  // namespace satori