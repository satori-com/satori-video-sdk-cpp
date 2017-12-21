#pragma once

#include <cbor.h>
#include <ostream>

#include "streams/streams.h"

namespace satori {
namespace video {
namespace streams {

streams::observer<cbor_item_t *> &ostream_sink(std::ostream &out);
}
}  // namespace video
}  // namespace satori