#include "cbor_json.h"

#include <cbor.h>
#include <gsl/gsl>
#include <limits>
#include <string>

#include "base64.h"
#include "logging.h"

namespace satori {
namespace video {

namespace {
cbor_item_t *json_to_cbor_item(const nlohmann::json &document) {
  if (document.is_string()) {
    return cbor_build_string(document.get<std::string>().c_str());
  }
  if (document.is_number_integer()) {
    cbor_item_t *result{nullptr};

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
    cbor_item_t *result{nullptr};

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
    cbor_item_t *message = cbor_new_definite_array(document.size());
    for (auto &el : document) {
      cbor_item_t *item = json_to_cbor_item(el);
      CHECK(cbor_array_push(message, cbor_move(item)));
    }
    return message;
  }
  if (document.is_object()) {
    cbor_item_t *message = cbor_new_definite_map(document.size());
    for (auto it = document.begin(); it != document.end(); ++it) {
      cbor_item_t *key = json_to_cbor_item(it.key());
      cbor_item_t *value = json_to_cbor_item(it.value());
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

nlohmann::json cbor_item_to_json(const cbor_item_t *item) {
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
        return std::string{reinterpret_cast<char *>(cbor_string_handle(item)),
                           cbor_string_length(item)};
      }

      if (cbor_string_is_indefinite(item)) {
        const size_t chunk_count = cbor_string_chunk_count(item);
        cbor_item_t **chunk_handle = cbor_string_chunks_handle(item);

        std::ostringstream result;
        for (size_t i = 0; i < chunk_count; i++) {
          cbor_item_t *chunk = chunk_handle[i];
          CHECK(cbor_string_is_definite(chunk));

          result << std::string{reinterpret_cast<char *>(cbor_string_handle(chunk)),
                                cbor_string_length(chunk)};
        }

        return result.str();
      }

      ABORT() << "Unreachable statement for string";
      return nullptr;
    }

    case CBOR_TYPE_ARRAY: {
      nlohmann::json array = nlohmann::json::array();
      auto handle = cbor_array_handle(item);
      for (size_t i = 0; i < cbor_array_size(item); i++) {
        array.emplace_back(cbor_item_to_json(handle[i]));
      }
      return array;
    }

    case CBOR_TYPE_MAP: {
      nlohmann::json map = nlohmann::json::object();
      auto handle = cbor_map_handle(item);
      for (size_t i = 0; i < cbor_map_size(item); i++) {
        map.emplace(cbor_item_to_json(handle[i].key), cbor_item_to_json(handle[i].value));
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

}  // namespace

std::string json_to_cbor(const nlohmann::json &document) {
  cbor_item_t *item = json_to_cbor_item(document);
  CHECK_EQ(1, cbor_refcount(item));

  auto decref_item = gsl::finally([&item]() { cbor_decref(&item); });

  uint8_t *buffer{nullptr};
  size_t buffer_size_ignore{0};
  const size_t buffer_length = cbor_serialize_alloc(item, &buffer, &buffer_size_ignore);

  CHECK_NOTNULL(buffer) << "failed to allocate cbor buffer: null";
  CHECK_GT(buffer_length, 0) << "failed to allocate cbor buffer: " << buffer_length;

  auto free_buffer = gsl::finally([buffer]() { free(buffer); });

  return std::string{buffer, buffer + buffer_length};
}

streams::error_or<nlohmann::json> cbor_to_json(const std::string &data) {
  cbor_load_result load_result{0};
  cbor_item_t *loaded_item =
      cbor_load(reinterpret_cast<cbor_data>(data.data()), data.size(), &load_result);

  if (load_result.error.code == CBOR_ERR_NONE) {
    CHECK_NOTNULL(loaded_item);
    CHECK_EQ(1, cbor_refcount(loaded_item));
    auto item_decref = gsl::finally([&loaded_item]() { cbor_decref(&loaded_item); });
    return cbor_item_to_json(loaded_item);
  }

  // If we get here, then there was a CBOR parsing error
  CHECK(loaded_item == nullptr);
  std::string error_message;
  switch (load_result.error.code) {
    case CBOR_ERR_NOTENOUGHDATA:
      error_message = "not enough data";
      break;
    case CBOR_ERR_NODATA:
      error_message = "no data";
      break;
    case CBOR_ERR_MALFORMATED:
      error_message = "malformed data";
      break;
    case CBOR_ERR_MEMERROR:
      error_message = "memory error";
      break;
    case CBOR_ERR_SYNTAXERROR:
      error_message = "syntax error";
      break;
    default:
      ABORT() << "unexpected case: " << load_result.error.code;
      break;
  }
  LOG(ERROR) << "Parse error: " << error_message << " at position "
             << load_result.error.position << ", read " << load_result.read << " bytes"
             << ", message: " << encode64(data);

  return std::system_category().default_error_condition(EBADMSG);
}

}  // namespace video
}  // namespace satori
