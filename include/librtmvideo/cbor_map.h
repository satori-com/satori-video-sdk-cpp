// Reading values from CBOR maps
#pragma once
#include <cbor.h>

extern "C" {
// Returns string value for a key or default value if key is not found or not a string.
const char *cbor_map_get_str(cbor_item_t *map, const char *name,
                             const char *default_value = nullptr);

// Returns boolean flag indicating if key/value pair exists.
int cbor_map_has_str_value(cbor_item_t *map, const char *name,
                           const char *value);

// Returns CBOR value for a key or default value if key is not found.
cbor_item_t *cbor_map_get(cbor_item_t *map, const char *name,
                          cbor_item_t *default_value = nullptr);
}
