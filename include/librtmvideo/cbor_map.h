#pragma once
#include <cbor.h>

extern "C" {
const char *cbor_map_get_str(cbor_item_t *map, const char *name,
                             const char *default_value = nullptr);
int cbor_map_has_str_value(cbor_item_t *map, const char *name,
                           const char *value);
cbor_item_t *cbor_map_get(cbor_item_t *map, const char *name,
                          cbor_item_t *default_value = nullptr);
}
