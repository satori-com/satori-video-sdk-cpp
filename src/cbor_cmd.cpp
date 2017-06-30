#include "librtmvideo/cbor_cmd.h"
#include <string.h>

cbor_item_t *cbor_lookup_by_key(cbor_item_t *map, const char *name) {
  if (map != nullptr && cbor_map_is_definite(map)) {
    for (size_t i = 0; i < cbor_map_size(map); i++) {
      if (strcmp(name, reinterpret_cast<char *>(cbor_string_handle(
                           cbor_map_handle(map)[i].key))) == 0) {
        return cbor_map_handle(map)[i].value;
      }
    }
  }
  return nullptr;
}

int bot_cmd_check_action(cbor_item_t *command, const char *name) {
  cbor_item_t *value = cbor_lookup_by_key(command, "action");
  if (value == nullptr) return 0;
  if (strcmp(name, reinterpret_cast<char *>(cbor_string_handle(value))) == 0) {
    return 1;
  }
  return 0;
}

const char *bot_cmd_get_str_value(cbor_item_t *command, const char *name,
                                  const char *default_value) {
  cbor_item_t *value =
      cbor_lookup_by_key(cbor_lookup_by_key(command, "body"), name);
  if (value == nullptr) return default_value;
  return reinterpret_cast<char *>(cbor_string_handle(value));
}
