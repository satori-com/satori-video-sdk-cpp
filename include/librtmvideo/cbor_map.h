// Reading values from CBOR maps
#pragma once
#include <cbor.h>
#include <string>

namespace cbor_helpers {
// Returns string value for a key or default value if key is not found or not a
// string.
std::string map_get_str(cbor_item_t *map, const std::string name,
                        const std::string default_value);

// Returns boolean flag indicating if key/value pair exists.
bool map_has_str_value(cbor_item_t *map, const std::string name,
                       const std::string value);

// Returns CBOR value for a key or default value if key is not found.
cbor_item_t *map_get(cbor_item_t *map, const std::string name,
                     cbor_item_t *default_value = nullptr);
}  // namespace cbor_helpers
