#include "cbor_json.h"
#include <iostream>

#include "logging.h"

namespace satori {
namespace video {

cbor_item_t* json_to_cbor(const rapidjson::Value& d) {
  if (d.IsString()) return cbor_build_string(d.GetString());
  if (d.IsInt()) {
    int i = d.GetInt();
    return i > 0 ? cbor_build_uint32(i) : cbor_build_negint32(static_cast<uint32_t>(-i));
  }
  if (d.IsInt64()) {
    int64_t i = d.GetInt64();
    return i > 0 ? cbor_build_uint64(i) : cbor_build_negint64(static_cast<uint64_t>(-i));
  }
  if (d.IsDouble()) return cbor_build_float8(d.GetDouble());
  if (d.IsArray()) {
    cbor_item_t* message = cbor_new_definite_array(d.Size());
    for (auto& m : d.GetArray())
      if (!cbor_array_push(message, cbor_move(json_to_cbor(m))))
        std::cerr << "ERROR: Failed to push to array\n";
    return message;
  }
  if (d.IsObject()) {
    cbor_item_t* message = cbor_new_definite_map(d.MemberCount());
    for (auto& m : d.GetObject()) {
      cbor_map_add(message,
                   {cbor_move(json_to_cbor(m.name)), cbor_move(json_to_cbor(m.value))});
    }
    return message;
  }
  if (d.IsBool()) return cbor_build_bool(d.GetBool());
  if (d.IsFalse()) return cbor_build_bool(false);
  if (d.IsUint()) return cbor_build_uint32(d.GetUint());
  if (d.IsUint64()) return cbor_build_uint64(d.GetUint64());

  ABORT() << "Unsupported message field";
  return cbor_build_bool(false);
}

rapidjson::Value cbor_to_json(const cbor_item_t* item, rapidjson::Document& document) {
  rapidjson::Value a;
  switch (cbor_typeof(item)) {
    case CBOR_TYPE_NEGINT:
      a = rapidjson::Value(-cbor_get_int(item));
    break;
    case CBOR_TYPE_UINT:
      a = rapidjson::Value(cbor_get_int(item));
      break;
    case CBOR_TYPE_TAG:
    case CBOR_TYPE_BYTESTRING:
      ABORT() << "NOT IMPLEMENTED";
      break;
    case CBOR_TYPE_STRING:
      if (cbor_string_is_indefinite(item)) {
        ABORT() << "NOT IMPLEMENTED";
      } else {
        // unsigned char * -> char *
        a.SetString(reinterpret_cast<char*>(cbor_string_handle(item)),
                    static_cast<int>(cbor_string_length(item)), document.GetAllocator());
      }
      break;
    case CBOR_TYPE_ARRAY:
      a = rapidjson::Value(rapidjson::kArrayType);
      for (size_t i = 0; i < cbor_array_size(item); i++)
        a.PushBack(cbor_to_json(cbor_array_handle(item)[i], document),
                   document.GetAllocator());
      break;
    case CBOR_TYPE_MAP:
      a = rapidjson::Value(rapidjson::kObjectType);
      for (size_t i = 0; i < cbor_map_size(item); i++)
        a.AddMember(cbor_to_json(cbor_map_handle(item)[i].key, document),
                    cbor_to_json(cbor_map_handle(item)[i].value, document),
                    document.GetAllocator());
      break;
    case CBOR_TYPE_FLOAT_CTRL:
      a = rapidjson::Value(cbor_float_get_float(item));
      break;
  }
  return a;
}

}  // namespace video
}  // namespace satori
