// Reading values from CBOR maps
#pragma once
#include <cbor.h>
#include <iostream>

namespace cbor {
void dump(std::ostream &out, const cbor_item_t *item);
}  // namespace cbor
