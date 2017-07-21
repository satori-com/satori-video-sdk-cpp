#include "librtmvideo/cbor_map.h"
#include <string.h>

namespace cbor_helpers {
cbor_item_t *map_get(cbor_item_t *map, const char *name,
                     cbor_item_t *default_value) {
  if (map != nullptr && cbor_map_is_definite(map)) {
    for (size_t i = 0; i < cbor_map_size(map); i++) {
      if (strcmp(name, reinterpret_cast<char *>(cbor_string_handle(
                           cbor_map_handle(map)[i].key))) == 0) {
        return cbor_map_handle(map)[i].value;
      }
    }
  }
  return default_value;
}

bool map_has_str_value(cbor_item_t *map, const char *name, const char *value) {
  cbor_item_t *item = map_get(map, name);
  if (item == nullptr) return 0;
  if (cbor_string_length(item) != strlen(value)) return false;
  if (strncmp(value, reinterpret_cast<char *>(cbor_string_handle(item)),
              cbor_string_length(item)) == 0) {
    return true;
  }
  return false;
}

const std::string map_get_str(cbor_item_t *map, const char *name,
                              const std::string default_value) {
  cbor_item_t *value = map_get(map, name);
  if (value == nullptr) return default_value;
  return std::string(reinterpret_cast<char *>(cbor_string_handle(value)),
                     cbor_string_length(value));
}
}  // namespace cbor_helpers
