// Reading values from CBOR maps
#pragma once
#include <cbor.h>
#include <iostream>
#include <string>

namespace cbor {
// Returns string value for a key or default value if key is not found or not a
// string.
std::string map_get_str(const cbor_item_t *map, const std::string &name,
                        const std::string &default_value);

// Returns int value for a key or default value if key is not found or not an
// int.
int map_get_int(const cbor_item_t *map, const std::string &name, int default_value);

// Returns boolean flag indicating if key/value pair exists.
bool map_has_str_value(const cbor_item_t *map, const std::string &name,
                       const std::string &value);

// Returns CBOR value for a key or default value if key is not found.
cbor_item_t *map_get(const cbor_item_t *map, const std::string &name,
                     cbor_item_t *default_value = nullptr);

int64_t get_int64(const cbor_item_t *item);

double get_double(const cbor_item_t *item);

std::string get_string(const cbor_item_t *item);

struct map {
  explicit map(const cbor_item_t *item) : item(item) {}

  cbor_item_t *get(const std::string &key, cbor_item_t *default_value = nullptr) const {
    return map_get(item, key, default_value);
  }

  const map get_map(const std::string &key, cbor_item_t *default_value = nullptr) const {
    return map(get(key, default_value));
  }

  std::string get_str(const std::string &key,
                      const std::string &default_value = "") const {
    return map_get_str(item, key, default_value);
  }

  const cbor_item_t *item;
};
}  // namespace cbor

std::ostream &operator<<(std::ostream &out, const cbor_item_t *item);