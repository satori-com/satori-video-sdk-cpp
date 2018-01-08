#pragma once

#include <json.hpp>
#include <string>

#include "streams/error_or.h"

namespace satori {
namespace video {

std::string json_to_cbor(const nlohmann::json& document);

streams::error_or<nlohmann::json> cbor_to_json(const std::string& data);

}  // namespace video
}  // namespace satori
