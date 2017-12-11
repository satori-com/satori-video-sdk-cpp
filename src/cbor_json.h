#pragma once

#include <cbor.h>
#include <json.hpp>

namespace satori {
namespace video {

cbor_item_t* json_to_cbor(const nlohmann::json& document);

nlohmann::json cbor_to_json(const cbor_item_t* item);

}  // namespace video
}  // namespace satori
