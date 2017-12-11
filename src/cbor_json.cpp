#include "cbor_json.h"
#include <iostream>

#include "logging.h"

namespace satori {
namespace video {

cbor_item_t* json_to_cbor(const rapidjson::Value& d) {
  if (d.IsString()) {
    return cbor_build_string(d.GetString());
  }
  if (d.IsInt()) {
    int i = d.GetInt();
    return i >= 0 ? cbor_build_uint32(i)
                  : cbor_build_negint32(static_cast<uint32_t>(-1 - i));
  }
  if (d.IsInt64()) {
    int64_t i = d.GetInt64();
    return i >= 0 ? cbor_build_uint64(i)
                  : cbor_build_negint64(static_cast<uint64_t>(-1 - i));
  }
  if (d.IsDouble()) {
    return cbor_build_float8(d.GetDouble());
  }
  if (d.IsArray()) {
    cbor_item_t* message = cbor_new_definite_array(d.Size());
    for (auto& m : d.GetArray()) {
      cbor_item_t* item = json_to_cbor(m);
      CHECK_EQ(1, cbor_refcount(item));
      CHECK(cbor_array_push(message, cbor_move(item)));
    }
    return message;
  }
  if (d.IsObject()) {
    cbor_item_t* message = cbor_new_definite_map(d.MemberCount());
    for (auto& m : d.GetObject()) {
      cbor_item_t* key = json_to_cbor(m.name);
      cbor_item_t* value = json_to_cbor(m.value);
      CHECK_EQ(1, cbor_refcount(key));
      CHECK_EQ(1, cbor_refcount(value));
      CHECK(cbor_map_add(message, {cbor_move(key), cbor_move(value)}));
    }
    return message;
  }
  if (d.IsBool()) {
    return cbor_build_bool(d.GetBool());
  }
  if (d.IsFalse()) {
    return cbor_build_bool(false);
  }
  if (d.IsUint()) {
    return cbor_build_uint32(d.GetUint());
  }
  if (d.IsUint64()) {
    return cbor_build_uint64(d.GetUint64());
  }
  if (d.IsNull()) {
    return cbor_new_null();
  }

  ABORT() << "Unsupported message field";
  return cbor_build_bool(false);
}

rapidjson::Value cbor_to_json(const cbor_item_t* item, rapidjson::Document& document) {
  rapidjson::Value result;

  switch (cbor_typeof(item)) {
    case CBOR_TYPE_UINT:
      result = rapidjson::Value(cbor_get_int(item));
      break;
    case CBOR_TYPE_NEGINT:
      result = rapidjson::Value(-1 - static_cast<int64_t>(cbor_get_int(item)));
      break;
    case CBOR_TYPE_BYTESTRING:
      ABORT() << "CBOR byte strings are not supported";
      break;
    case CBOR_TYPE_STRING:
      result.SetString(reinterpret_cast<char*>(cbor_string_handle(item)),
                       static_cast<int>(cbor_string_length(item)),
                       document.GetAllocator());
      break;
    case CBOR_TYPE_ARRAY:
      result = rapidjson::Value(rapidjson::kArrayType);
      for (size_t i = 0; i < cbor_array_size(item); i++) {
        result.PushBack(cbor_to_json(cbor_array_handle(item)[i], document),
                        document.GetAllocator());
      }
      break;
    case CBOR_TYPE_MAP:
      result = rapidjson::Value(rapidjson::kObjectType);
      for (size_t i = 0; i < cbor_map_size(item); i++) {
        result.AddMember(cbor_to_json(cbor_map_handle(item)[i].key, document),
                         cbor_to_json(cbor_map_handle(item)[i].value, document),
                         document.GetAllocator());
      }
      break;
    case CBOR_TYPE_TAG:
      ABORT() << "CBOR tags are not supported";
      break;
    case CBOR_TYPE_FLOAT_CTRL:
      if (cbor_is_float(item)) {
        result = rapidjson::Value(cbor_float_get_float(item));
      } else if (cbor_is_null(item)) {
        // default rapidjson value is null
      } else if (cbor_is_bool(item)) {
        return rapidjson::Value(cbor_ctrl_is_bool(item));
      } else {
        ABORT() << "not supported float or control";
      }
      break;
    default:
      ABORT() << "Not supported";
  }

  return result;
}

}  // namespace video
}  // namespace satori
