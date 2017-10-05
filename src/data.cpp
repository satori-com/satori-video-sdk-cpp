#include <cmath>

#include "base64.h"
#include "data.h"

std::ostream &operator<<(std::ostream &os, const frame_id &id) {
  os << "[" << id.i1 << ", " << id.i2 << "]";
  return os;
}

namespace rtm {
namespace video {

cbor_item_t *network_frame::to_cbor() const {
  cbor_item_t *root = cbor_new_definite_map(6);

  cbor_map_add(root, {cbor_move(cbor_build_string("d")),
                      cbor_move(cbor_build_string(base64_data.c_str()))});

  cbor_item_t *ids = cbor_new_definite_array(2);
  cbor_array_set(ids, 0, cbor_build_uint64(id.i1));
  cbor_array_set(ids, 1, cbor_build_uint64(id.i2));
  cbor_map_add(root, {cbor_move(cbor_build_string("i")), cbor_move(ids)});

  cbor_map_add(root, {cbor_move(cbor_build_string("t")),
                      cbor_move(cbor_build_float8(t.time_since_epoch().count()))});

  cbor_map_add(root, {cbor_move(cbor_build_string("rt")),
                      cbor_move(cbor_build_uint64(t.time_since_epoch().count()))});

  cbor_map_add(root,
               {cbor_move(cbor_build_string("c")), cbor_move(cbor_build_uint8(chunk))});

  cbor_map_add(root,
               {cbor_move(cbor_build_string("l")), cbor_move(cbor_build_uint8(chunks))});

  return root;
}

cbor_item_t *network_metadata::to_cbor() const {
  cbor_item_t *root = cbor_new_definite_map(2);

  cbor_map_add(root, {cbor_move(cbor_build_string("codecName")),
                      cbor_move(cbor_build_string(codec_name.c_str()))});

  cbor_map_add(root, {cbor_move(cbor_build_string("codecData")),
                      cbor_move(cbor_build_string(base64_data.c_str()))});

  return root;
}

network_metadata encoded_metadata::to_network() const {
  network_metadata nm;

  nm.codec_name = codec_name;
  if (codec_data.size() > 0) {
    nm.base64_data = std::move(rtm::video::encode64(codec_data));
  }

  return nm;
}

std::vector<network_frame> encoded_frame::to_network(
    std::chrono::system_clock::time_point t) const {
  std::vector<network_frame> frames;

  std::string encoded = std::move(rtm::video::encode64(data));
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
}  // namespace rtm