#pragma once

#include <cbor.h>
#include <rapidjson/document.h>

namespace rtm {
namespace video {

cbor_item_t* json_to_cbor(const rapidjson::Value& d);

rapidjson::Value cbor_to_json(const cbor_item_t* item, rapidjson::Document& document);
}  // namespace video
}  // namespace rtm
