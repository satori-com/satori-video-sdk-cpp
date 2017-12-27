#include "cbor_tools.h"

#include <cstring>

#include "logging.h"

namespace cbor {
cbor_item_t *map_get(const cbor_item_t *map, const std::string &name,
                     cbor_item_t *default_value) {
  CHECK(cbor_isa_map(map));
  if (map != nullptr) {
    for (size_t i = 0; i < cbor_map_size(map); i++) {
      cbor_item_t *key = cbor_map_handle(map)[i].key;
      if (name == get_string(key)) {
        return cbor_map_handle(map)[i].value;
      }
    }
  }
  return default_value;
}

bool map_has_str_value(const cbor_item_t *map, const std::string &name,
                       const std::string &value) {
  const cbor_item_t *item = map_get(map, name);
  if (item == nullptr) {
    return false;
  }
  if (cbor_string_length(item) != value.length()) {
    return false;
  }
  return value == get_string(item);
}

std::string map_get_str(const cbor_item_t *map, const std::string &name,
                        const std::string &default_value) {
  const cbor_item_t *value = map_get(map, name);
  if (value == nullptr) {
    return default_value;
  }
  return get_string(value);
}

int map_get_int(const cbor_item_t *map, const std::string &name,
                const int default_value) {
  const cbor_item_t *value = map_get(map, name);
  if (value == nullptr) {
    return default_value;
  }
  return cbor_get_int(value);
}

void dump_as_json(std::ostream &out, const cbor_item_t *item) {
  switch (cbor_typeof(item)) {
    case CBOR_TYPE_NEGINT:
      out << '-' << cbor_get_int(item);
      break;
    case CBOR_TYPE_UINT:
      out << cbor_get_int(item);
      break;
    case CBOR_TYPE_TAG:
    case CBOR_TYPE_BYTESTRING:
      ABORT();
      break;
    case CBOR_TYPE_STRING:
      if (cbor_string_is_indefinite(item)) {
        ABORT();
      } else {
        out << '"' << get_string(item) << '"';
      }
      break;
    case CBOR_TYPE_ARRAY:
      out << '[';
      for (size_t i = 0; i < cbor_array_size(item); i++) {
        if (i > 0) {
          out << ',';
        }
        dump_as_json(out, cbor_array_handle(item)[i]);
      }
      out << ']';
      break;
    case CBOR_TYPE_MAP:
      out << '{';
      for (size_t i = 0; i < cbor_map_size(item); i++) {
        if (i > 0) {
          out << ',';
        }
        dump_as_json(out, cbor_map_handle(item)[i].key);
        out << ':';
        dump_as_json(out, cbor_map_handle(item)[i].value);
      }
      out << '}';
      break;
    case CBOR_TYPE_FLOAT_CTRL:
      if (cbor_is_float(item)) {
        out << cbor_float_get_float(item);
        return;
      }
      if (cbor_is_null(item)) {
        out << "null";
        return;
      }
      if (cbor_is_bool(item)) {
        out << cbor_ctrl_is_bool(item);
        return;
      }
      ABORT() << "not implemented for float control";
      break;
  }
}

uint64_t get_uint64(const cbor_item_t *item) {
  CHECK(!cbor_isa_negint(item));
  return cbor_get_int(item);
}

cbor_item_t *build_int64(int64_t value) {
  cbor_item_t *res = cbor_build_uint64(abs(value));
  if (value < 0) {
    cbor_mark_negint(res);
  }
  return res;
}

int64_t get_int64(const cbor_item_t *item) {
  if (cbor_isa_negint(item)) {
    return -cbor_get_int(item);
  }

  return cbor_get_int(item);
}

double get_double(const cbor_item_t *item) {
  if (cbor_is_int(item)) {
    return static_cast<double>(get_int64(item));
  }
  return cbor_float_get_float(item);
}

std::string get_string(const cbor_item_t *item) {
  CHECK(cbor_isa_string(item));
  return std::string{reinterpret_cast<char *>(cbor_string_handle(item)),
                     cbor_string_length(item)};
}

}  // namespace cbor

std::ostream &operator<<(std::ostream &out, const cbor_item_t *item) {
  cbor::dump_as_json(out, item);
  return out;
}
