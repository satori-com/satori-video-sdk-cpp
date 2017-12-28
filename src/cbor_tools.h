#pragma once

#include <cbor.h>
#include <iostream>

std::ostream &operator<<(std::ostream &out, const cbor_item_t *item);
