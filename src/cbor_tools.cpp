#include "librtmvideo/cbor_tools.h"
#include <string.h>

namespace cbor {
cbor_item_t *map_get(cbor_item_t *map, const std::string name,
                     cbor_item_t *default_value) {
  if (map != nullptr && cbor_map_is_definite(map)) {
    for (size_t i = 0; i < cbor_map_size(map); i++) {
      cbor_item_t *key = cbor_map_handle(map)[i].key;
      if (strncmp(name.c_str(), reinterpret_cast<char *>(cbor_string_handle(key)),
                  cbor_string_length(key)) == 0) {
        return cbor_map_handle(map)[i].value;
      }
    }
  }
  return default_value;
}

bool map_has_str_value(cbor_item_t *map, const std::string name,
                       const std::string value) {
  cbor_item_t *item = map_get(map, name);
  if (item == nullptr) return 0;
  if (cbor_string_length(item) != value.length()) return false;
  return strncmp(value.c_str(), reinterpret_cast<char *>(cbor_string_handle(item)),
                 cbor_string_length(item)) == 0;
}

std::string map_get_str(cbor_item_t *map, const std::string name,
                        const std::string default_value) {
  cbor_item_t *value = map_get(map, name);
  if (value == nullptr) return default_value;
  return std::string(reinterpret_cast<char *>(cbor_string_handle(value)),
                     cbor_string_length(value));
}

int map_get_int(cbor_item_t *map, const std::string name, const int default_value) {
  cbor_item_t *value = map_get(map, name);
  if (value == nullptr) return default_value;
  return cbor_get_int(value);
}

void dump(std::ostream &out, const cbor_item_t *item) {
  switch (cbor_typeof(item)) {
    case CBOR_TYPE_NEGINT:
      out << '-' << cbor_get_int(item);
      break;
    case CBOR_TYPE_UINT:
      out << cbor_get_int(item);
      break;
    case CBOR_TYPE_TAG:
    case CBOR_TYPE_BYTESTRING:
      assert(false);
      break;
    case CBOR_TYPE_STRING:
      if (cbor_string_is_indefinite(item)) {
        assert(false);
      } else {
        std::string a(reinterpret_cast<char *>(cbor_string_handle(item)),
                      static_cast<int>(cbor_string_length(item)));
        out << '"' << a << '"';
      }
      break;
    case CBOR_TYPE_ARRAY:
      out << '[';
      for (size_t i = 0; i < cbor_array_size(item); i++) {
        if (i > 0) out << ',';
        dump(out, cbor_array_handle(item)[i]);
      }
      out << ']';
      break;
    case CBOR_TYPE_MAP:
      out << '{';
      for (size_t i = 0; i < cbor_map_size(item); i++) {
        if (i > 0) out << ',';
        dump(out, cbor_map_handle(item)[i].key);
        out << ':';
        dump(out, cbor_map_handle(item)[i].value);
      }
      out << '}';
      break;
    case CBOR_TYPE_FLOAT_CTRL:
      out << cbor_float_get_float(item);
      break;
  }
}
}  // namespace cbor
