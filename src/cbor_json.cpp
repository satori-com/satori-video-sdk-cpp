#include "cbor_json.h"

#include <limits>
#include <string>

#include "logging.h"

namespace satori {
namespace video {

cbor_item_t* rapidjson_to_cbor(const rapidjson::Value& d) {
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
      cbor_item_t* item = rapidjson_to_cbor(m);
      CHECK_EQ(1, cbor_refcount(item));
      CHECK(cbor_array_push(message, cbor_move(item)));
    }
    return message;
  }
  if (d.IsObject()) {
    cbor_item_t* message = cbor_new_definite_map(d.MemberCount());
    for (auto& m : d.GetObject()) {
      cbor_item_t* key = rapidjson_to_cbor(m.name);
      cbor_item_t* value = rapidjson_to_cbor(m.value);
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

rapidjson::Value cbor_to_rapidjson(const cbor_item_t* item,
                                   rapidjson::Document& document) {
  switch (cbor_typeof(item)) {
    case CBOR_TYPE_UINT: {
      return rapidjson::Value(cbor_get_int(item));
    }

    case CBOR_TYPE_NEGINT: {
      return rapidjson::Value(-1 - static_cast<int64_t>(cbor_get_int(item)));
    }

    case CBOR_TYPE_BYTESTRING: {
      ABORT() << "CBOR byte strings are not supported";
      rapidjson::Value null_value;
      return null_value;
    }

    case CBOR_TYPE_STRING: {
      if (cbor_string_is_definite(item)) {
        rapidjson::Value result;
        result.SetString(reinterpret_cast<char*>(cbor_string_handle(item)),
                         static_cast<int>(cbor_string_length(item)),
                         document.GetAllocator());
        return result;
      }

      if (cbor_string_is_indefinite(item)) {
        const size_t chunk_count = cbor_string_chunk_count(item);
        cbor_item_t** chunk_handle = cbor_string_chunks_handle(item);

        std::ostringstream result_stream;
        for (size_t i = 0; i < chunk_count; i++) {
          cbor_item_t* chunk = chunk_handle[i];
          CHECK(cbor_string_is_definite(chunk));

          result_stream << std::string{reinterpret_cast<char*>(cbor_string_handle(chunk)),
                                       cbor_string_length(chunk)};
        }
        const std::string result = result_stream.str();

        return rapidjson::Value(result.data(), static_cast<int>(result.size()),
                                document.GetAllocator());
      }

      ABORT() << "Unreachable statement for string";
      rapidjson::Value null_value;
      return null_value;
    }

    case CBOR_TYPE_ARRAY: {
      rapidjson::Value array;
      array = rapidjson::Value(rapidjson::kArrayType);
      for (size_t i = 0; i < cbor_array_size(item); i++) {
        array.PushBack(cbor_to_rapidjson(cbor_array_handle(item)[i], document),
                       document.GetAllocator());
      }
      return array;
    }

    case CBOR_TYPE_MAP: {
      rapidjson::Value map;
      map = rapidjson::Value(rapidjson::kObjectType);
      for (size_t i = 0; i < cbor_map_size(item); i++) {
        map.AddMember(cbor_to_rapidjson(cbor_map_handle(item)[i].key, document),
                      cbor_to_rapidjson(cbor_map_handle(item)[i].value, document),
                      document.GetAllocator());
      }
      return map;
    }

    case CBOR_TYPE_TAG: {
      ABORT() << "CBOR tags are not supported";
      rapidjson::Value null_value;
      return null_value;
    }

    case CBOR_TYPE_FLOAT_CTRL: {
      if (cbor_is_float(item)) {
        return rapidjson::Value(cbor_float_get_float(item));
      }
      if (cbor_is_null(item)) {
        rapidjson::Value null_value;
        return null_value;
      }
      if (cbor_is_bool(item)) {
        return rapidjson::Value(cbor_ctrl_is_bool(item));
      }
      ABORT() << "not supported float or control";
      rapidjson::Value null_value;
      return null_value;
    }

    default: {
      ABORT() << "Not supported";
      rapidjson::Value null_value;
      return null_value;
    }
  }
}

cbor_item_t* json_to_cbor(const nlohmann::json& document) {
  if (document.is_string()) {
    return cbor_build_string(document.get<std::string>().c_str());
  }
  if (document.is_number_integer()) {
    cbor_item_t* result{nullptr};

    const auto i = document.get<int64_t>();
    if (i >= 0) {
      if (i <= std::numeric_limits<uint8_t>::max()) {
        result = cbor_build_uint8(static_cast<uint8_t>(i));
      } else if (i <= std::numeric_limits<uint16_t>::max()) {
        result = cbor_build_uint16(static_cast<uint16_t>(i));
      } else if (i <= std::numeric_limits<uint32_t>::max()) {
        result = cbor_build_uint32(static_cast<uint32_t>(i));
      } else {
        result = cbor_build_uint64(static_cast<uint64_t>(i));
      }
    } else {
      const auto p = -1 - i;  // https://tools.ietf.org/html/rfc7049#section-2.1
      if (p <= std::numeric_limits<uint8_t>::max()) {
        result = cbor_build_negint8(static_cast<uint8_t>(p));
      } else if (p <= std::numeric_limits<uint16_t>::max()) {
        result = cbor_build_negint16(static_cast<uint16_t>(p));
      } else if (p <= std::numeric_limits<uint32_t>::max()) {
        result = cbor_build_negint32(static_cast<uint32_t>(p));
      } else {
        result = cbor_build_negint64(static_cast<uint64_t>(p));
      }
    }

    CHECK_NOTNULL(result);
    return result;
  }
  if (document.is_number_unsigned()) {
    cbor_item_t* result{nullptr};

    const auto i = document.get<uint64_t>();
    if (i <= std::numeric_limits<uint8_t>::max()) {
      result = cbor_build_uint8(static_cast<uint8_t>(i));
    } else if (i <= std::numeric_limits<uint16_t>::max()) {
      result = cbor_build_uint16(static_cast<uint16_t>(i));
    } else if (i <= std::numeric_limits<uint32_t>::max()) {
      result = cbor_build_uint32(static_cast<uint32_t>(i));
    } else {
      result = cbor_build_uint64(static_cast<uint64_t>(i));
    }

    CHECK_NOTNULL(result);
    return result;
  }
  if (document.is_number_float()) {
    return cbor_build_float8(document.get<double>());
  }
  if (document.is_array()) {
    cbor_item_t* message = cbor_new_definite_array(document.size());
    for (auto& el : document) {
      cbor_item_t* item = json_to_cbor(el);
      CHECK(cbor_array_push(message, cbor_move(item)));
    }
    return message;
  }
  if (document.is_object()) {
    cbor_item_t* message = cbor_new_definite_map(document.size());
    for (auto it = document.begin(); it != document.end(); ++it) {
      cbor_item_t* key = json_to_cbor(it.key());
      cbor_item_t* value = json_to_cbor(it.value());
      CHECK(cbor_map_add(message, {cbor_move(key), cbor_move(value)}));
    }
    return message;
  }
  if (document.is_boolean()) {
    return cbor_build_bool(document.get<bool>());
  }
  if (document.is_null()) {
    return cbor_new_null();
  }

  ABORT() << "Unsupported message field: " << document;
  return cbor_build_bool(false);
}

nlohmann::json cbor_to_json(const cbor_item_t* item) {
  switch (cbor_typeof(item)) {
    case CBOR_TYPE_UINT: {
      return cbor_get_int(item);
    }

    case CBOR_TYPE_NEGINT: {
      const uint64_t value = cbor_get_int(item);
      CHECK_LE(value, static_cast<uint64_t>(std::numeric_limits<int64_t>::max()));

      // https://tools.ietf.org/html/rfc7049#section-2.1
      return -1 - static_cast<int64_t>(value);
    }

    case CBOR_TYPE_BYTESTRING: {
      ABORT() << "CBOR byte strings are not supported";
      return nullptr;
    }

    case CBOR_TYPE_STRING: {
      if (cbor_string_is_definite(item)) {
        return std::string{reinterpret_cast<char*>(cbor_string_handle(item)),
                           cbor_string_length(item)};
      }

      if (cbor_string_is_indefinite(item)) {
        const size_t chunk_count = cbor_string_chunk_count(item);
        cbor_item_t** chunk_handle = cbor_string_chunks_handle(item);

        std::ostringstream result;
        for (size_t i = 0; i < chunk_count; i++) {
          cbor_item_t* chunk = chunk_handle[i];
          CHECK(cbor_string_is_definite(chunk));

          result << std::string{reinterpret_cast<char*>(cbor_string_handle(chunk)),
                                cbor_string_length(chunk)};
        }

        return result.str();
      }

      ABORT() << "Unreachable statement for string";
      return nullptr;
    }

    case CBOR_TYPE_ARRAY: {
      nlohmann::json array;
      for (size_t i = 0; i < cbor_array_size(item); i++) {
        array.emplace_back(cbor_to_json(cbor_array_handle(item)[i]));
      }
      return array;
    }

    case CBOR_TYPE_MAP: {
      nlohmann::json map;
      for (size_t i = 0; i < cbor_map_size(item); i++) {
        map.emplace(cbor_to_json(cbor_map_handle(item)[i].key),
                    cbor_to_json(cbor_map_handle(item)[i].value));
      }
      return map;
    }

    case CBOR_TYPE_TAG: {
      ABORT() << "CBOR tags are not supported";
      return nullptr;
    }

    case CBOR_TYPE_FLOAT_CTRL: {
      if (cbor_is_float(item)) {
        return cbor_float_get_float(item);
      }
      if (cbor_is_null(item)) {
        return nullptr;
      }
      if (cbor_is_bool(item)) {
        return cbor_ctrl_is_bool(item);
      }
      ABORT() << "not implemented for float control";
      return nullptr;
    }

    default: {
      ABORT() << "NOT IMPLEMENTED";
      return nullptr;
    }
  }
}

}  // namespace video
}  // namespace satori
