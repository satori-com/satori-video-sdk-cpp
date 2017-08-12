#include "cbor_json.h"
#include <iostream>

cbor_item_t* json_to_cbor(const rapidjson::Value& d) {
  if (d.IsString()) return cbor_build_string(d.GetString());
  if (d.IsInt()) {
    int n = d.GetInt();
    cbor_item_t* message = cbor_build_uint32(abs(n));
    if (n < 0) cbor_mark_negint(message);
    return message;
  }
  if (d.IsDouble()) return cbor_build_float8(d.GetDouble());
  if (d.IsArray()) {
    cbor_item_t* message = cbor_new_definite_array(d.Size());
    for (auto& m : d.GetArray())
      if (!cbor_array_push(message, cbor_move(json_to_cbor(m))))
        std::cerr << "ERROR: Failed to push to array\n";
    return message;
  }
  if (d.IsObject()) {
    cbor_item_t* message = cbor_new_definite_map(d.MemberCount());
    for (auto& m : d.GetObject()) {
      cbor_map_add(message, (struct cbor_pair){
                                .key = cbor_move(json_to_cbor(m.name)),
                                .value = cbor_move(json_to_cbor(m.value))});
    }
    return message;
  }
  std::cerr << "Unsupported message field\n";
  return cbor_build_bool(false);
}
