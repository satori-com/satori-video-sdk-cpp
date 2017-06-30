#include "librtmvideo/cbor_map.h"
#include <string.h>

cbor_item_t *cbor_map_get(cbor_item_t *map, const char *name,
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

int cbor_map_has_str_value(cbor_item_t *map, const char *name,
                           const char *value) {
  cbor_item_t *item = cbor_map_get(map, name);
  if (item == nullptr) return 0;
  if (strcmp(value, reinterpret_cast<char *>(cbor_string_handle(item))) == 0) {
    return 1;
  }
  return 0;
}

const char *cbor_map_get_str(cbor_item_t *map, const char *name,
                             const char *default_value) {
  cbor_item_t *value = cbor_map_get(map, name);
  if (value == nullptr) return default_value;
  return reinterpret_cast<char *>(cbor_string_handle(value));
}
