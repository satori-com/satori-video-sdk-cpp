#include <tuple>

#include "librtmvideo/rtmpacket.h"

namespace rtm {
namespace video {

cbor_item_t *network_frame::to_cbor() const {
  cbor_item_t *root = cbor_new_definite_map(6);
  cbor_map_add(root,
               {.key = cbor_move(cbor_build_string("d")),
                .value = cbor_move(cbor_build_string(base64_data.c_str()))});

  cbor_item_t *ids = cbor_new_definite_array(2);
  cbor_array_set(ids, 0, cbor_build_uint64(std::get<0>(id)));
  cbor_array_set(ids, 1, cbor_build_uint64(std::get<1>(id)));
  cbor_map_add(root, {.key = cbor_move(cbor_build_string("i")),
                      .value = cbor_move(ids)});

  cbor_map_add(
      root,
      {.key = cbor_move(cbor_build_string("t")),
       .value = cbor_move(cbor_build_float8(t.time_since_epoch().count()))});

  cbor_map_add(
      root,
      {.key = cbor_move(cbor_build_string("rt")),
       .value = cbor_move(cbor_build_uint64(t.time_since_epoch().count()))});

  cbor_map_add(root, {.key = cbor_move(cbor_build_string("c")),
                      .value = cbor_move(cbor_build_uint8(chunk))});

  cbor_map_add(root, {.key = cbor_move(cbor_build_string("l")),
                      .value = cbor_move(cbor_build_uint8(chunks))});

  return root;
}

cbor_item_t *network_metadata::to_cbor() const {
  cbor_item_t *root = cbor_new_definite_map(2);

  cbor_map_add(root,
               {.key = cbor_move(cbor_build_string("codecName")),
                .value = cbor_move(cbor_build_string(codec_name.c_str()))});

  cbor_map_add(root,
               {.key = cbor_move(cbor_build_string("codecData")),
                .value = cbor_move(cbor_build_string(base64_data.c_str()))});

  return root;
}

}  // namespace video
}  // namespace rtm