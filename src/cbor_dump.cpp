#include "librtmvideo/cbor_dump.h"
#include <string.h>

namespace cbor {
void dump(std::ostream &out, const cbor_item_t *item) {
  switch (cbor_typeof(item)) {
    case CBOR_TYPE_NEGINT:
      out << "-";
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
