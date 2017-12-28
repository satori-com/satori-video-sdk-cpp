#include "cbor_tools.h"

#include "logging.h"

namespace {

void dump_as_json(std::ostream &out, const cbor_item_t *item) {
  switch (cbor_typeof(item)) {
    case CBOR_TYPE_NEGINT:
      out << '-' << cbor_get_int(item);
      break;
    case CBOR_TYPE_UINT:
      out << cbor_get_int(item);
      break;
    case CBOR_TYPE_TAG:
    case CBOR_TYPE_BYTESTRING:
      ABORT();
      break;
    case CBOR_TYPE_STRING:
      if (cbor_string_is_indefinite(item)) {
        const size_t chunk_count = cbor_string_chunk_count(item);
        cbor_item_t **chunk_handle = cbor_string_chunks_handle(item);

        out << '"';
        for (size_t i = 0; i < chunk_count; i++) {
          cbor_item_t *chunk = chunk_handle[i];
          CHECK(cbor_string_is_definite(chunk));

          out << std::string{reinterpret_cast<char *>(cbor_string_handle(chunk)),
                             cbor_string_length(chunk)};
        }
        out << '"';
      } else {
        out << '"'
            << std::string{reinterpret_cast<char *>(cbor_string_handle(item)),
                           cbor_string_length(item)}
            << '"';
      }
      break;
    case CBOR_TYPE_ARRAY:
      out << '[';
      for (size_t i = 0; i < cbor_array_size(item); i++) {
        if (i > 0) {
          out << ',';
        }
        dump_as_json(out, cbor_array_handle(item)[i]);
      }
      out << ']';
      break;
    case CBOR_TYPE_MAP:
      out << '{';
      for (size_t i = 0; i < cbor_map_size(item); i++) {
        if (i > 0) {
          out << ',';
        }
        dump_as_json(out, cbor_map_handle(item)[i].key);
        out << ':';
        dump_as_json(out, cbor_map_handle(item)[i].value);
      }
      out << '}';
      break;
    case CBOR_TYPE_FLOAT_CTRL:
      if (cbor_is_float(item)) {
        out << cbor_float_get_float(item);
        return;
      }
      if (cbor_is_null(item)) {
        out << "null";
        return;
      }
      if (cbor_is_bool(item)) {
        out << cbor_ctrl_is_bool(item);
        return;
      }
      ABORT() << "not implemented for float control";
      break;
  }
}

}  // namespace

std::ostream &operator<<(std::ostream &out, const cbor_item_t *item) {
  dump_as_json(out, item);
  return out;
}
