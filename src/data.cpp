#include <cmath>

#include "base64.h"
#include "data.h"
#include "logging.h"

namespace satori {
namespace video {

cbor_item_t *network_frame::to_cbor() const {
  cbor_item_t *root = cbor_new_indefinite_map();

  cbor_map_add(root, {cbor_move(cbor_build_string("d")),
                      cbor_move(cbor_build_string(base64_data.c_str()))});

  cbor_item_t *ids = cbor_new_definite_array(2);
  cbor_array_set(ids, 0, cbor_move(cbor_build_uint64(id.i1)));
  cbor_array_set(ids, 1, cbor_move(cbor_build_uint64(id.i2)));
  cbor_map_add(root, {cbor_move(cbor_build_string("i")), cbor_move(ids)});

  cbor_map_add(root, {cbor_move(cbor_build_string("t")),
                      cbor_move(cbor_build_float8(t.time_since_epoch().count()))});

  cbor_map_add(root, {cbor_move(cbor_build_string("rt")),
                      cbor_move(cbor_build_uint64(t.time_since_epoch().count()))});

  cbor_map_add(root,
               {cbor_move(cbor_build_string("c")), cbor_move(cbor_build_uint8(chunk))});

  cbor_map_add(root,
               {cbor_move(cbor_build_string("l")), cbor_move(cbor_build_uint8(chunks))});

  return cbor_move(root);
}

cbor_item_t *network_metadata::to_cbor() const {
  cbor_item_t *root = cbor_new_indefinite_map();

  cbor_map_add(root, {cbor_move(cbor_build_string("codecName")),
                      cbor_move(cbor_build_string(codec_name.c_str()))});

  cbor_map_add(root, {cbor_move(cbor_build_string("codecData")),
                      cbor_move(cbor_build_string(base64_data.c_str()))});

  if (additional_data != nullptr) {
    CHECK(cbor_isa_map(additional_data));
    for (size_t i = 0; i < cbor_map_size(additional_data); i++) {
      cbor_map_add(root, {cbor_incref(cbor_map_handle(additional_data)[i].key),
                          cbor_incref(cbor_map_handle(additional_data)[i].value)});
    }
  }

  return cbor_move(root);
}

network_metadata encoded_metadata::to_network() const {
  network_metadata nm;

  nm.codec_name = codec_name;
  if (!codec_data.empty()) {
    nm.base64_data = std::move(satori::video::encode64(codec_data));
  }
  if (additional_data != nullptr) {
    nm.additional_data = cbor_incref(additional_data);
  }

  return nm;
}

std::vector<network_frame> encoded_frame::to_network(
    std::chrono::system_clock::time_point t) const {
  std::vector<network_frame> frames;

  std::string encoded = std::move(satori::video::encode64(data));
  size_t chunks = std::ceil((double)encoded.length() / max_payload_size);

  for (size_t i = 0; i < chunks; i++) {
    network_frame frame;
    frame.base64_data = encoded.substr(i * max_payload_size, max_payload_size);
    frame.id = id;
    frame.t = t;
    frame.chunk = static_cast<uint32_t>(i + 1);
    frame.chunks = static_cast<uint32_t>(chunks);

    frames.push_back(std::move(frame));
  }

  return frames;
}

}  // namespace video
}  // namespace satori

std::ostream &operator<<(std::ostream &out,
                         const satori::video::network_metadata &metadata) {
  out << "(codec_name=" << metadata.codec_name << ",base64_data=" << metadata.base64_data
      << ")";
  return out;
}

std::ostream &operator<<(std::ostream &out,
                         const satori::video::encoded_metadata &metadata) {
  out << metadata.to_network();
  return out;
}