#pragma once

#include <cbor.h>
#include <rapidjson/document.h>

cbor_item_t* json_to_cbor(const rapidjson::Value& d);
