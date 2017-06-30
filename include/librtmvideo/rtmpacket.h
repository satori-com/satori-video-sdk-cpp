#pragma once

#include <cbor.h>
#include <chrono>

namespace rtm {
namespace video {

static constexpr size_t max_payload_size = 65000;

cbor_item_t *frame_packet(
    std::string &&base64_data,
    uint64_t id1,
    uint64_t id2,
    std::chrono::system_clock::time_point &&time_point,
    size_t chunk_id,
    size_t nb_chunks) {
  cbor_item_t *root = cbor_new_definite_map(6);
  cbor_map_add(root,
              {.key = cbor_move(cbor_build_string("d")),
                .value = cbor_move(cbor_build_string(base64_data.c_str()))});

  cbor_item_t *ids = cbor_new_definite_array(2);
  cbor_array_set(ids, 0, cbor_build_uint64(id1));
  cbor_array_set(ids, 1, cbor_build_uint64(id2));
  cbor_map_add(root,
              {.key = cbor_move(cbor_build_string("i")),
                .value = cbor_move(ids)});

  cbor_map_add(root,
              {.key = cbor_move(cbor_build_string("t")),
                .value = cbor_move(cbor_build_float8(time_point.time_since_epoch().count()))});

  cbor_map_add(root,
              {.key = cbor_move(cbor_build_string("rt")),
                .value = cbor_move(cbor_build_float8(time_point.time_since_epoch().count()))});

  cbor_map_add(root,
              {.key = cbor_move(cbor_build_string("c")),
                .value = cbor_move(cbor_build_uint8(chunk_id))});

  cbor_map_add(root,
              {.key = cbor_move(cbor_build_string("l")),
                .value = cbor_move(cbor_build_uint8(nb_chunks))});

  return root;
}

cbor_item_t *metadata_packet(const std::string &codec_name, const std::string &base64_codec_data) {
  cbor_item_t *root = cbor_new_definite_map(2);

  cbor_map_add(root,
              {.key = cbor_move(cbor_build_string("codecName")),
                .value = cbor_move(cbor_build_string(codec_name.c_str()))});

  cbor_map_add(root,
              {.key = cbor_move(cbor_build_string("codecData")),
                .value = cbor_move(cbor_build_string(base64_codec_data.c_str()))});

  return root;
}

}
}