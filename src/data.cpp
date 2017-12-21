#include <cmath>
#include <gsl/gsl>

#include "base64.h"
#include "cbor_tools.h"
#include "data.h"
#include "logging.h"

namespace satori {
namespace video {

namespace {

double time_point_to_cbor(std::chrono::high_resolution_clock::time_point p) {
  auto duration = p.time_since_epoch();
  auto seconds_duration =
      std::chrono::duration_cast<std::chrono::duration<double>>(duration);
  double timestamp = seconds_duration.count();
  return timestamp;
}

}  // namespace
cbor_item_t *network_frame::to_cbor() const {
  cbor_item_t *root = cbor_new_indefinite_map();

  cbor_map_add(root, {cbor_move(cbor_build_string("d")),
                      cbor_move(cbor_build_string(base64_data.c_str()))});

  cbor_item_t *ids = cbor_new_definite_array(2);
  cbor_array_set(ids, 0, cbor_move(cbor_build_uint64(id.i1)));
  cbor_array_set(ids, 1, cbor_move(cbor_build_uint64(id.i2)));
  cbor_map_add(root, {cbor_move(cbor_build_string("i")), cbor_move(ids)});

  double timestamp = time_point_to_cbor(t);
  double departure_timestamp =
      time_point_to_cbor(std::chrono::high_resolution_clock::now());

  cbor_map_add(
      root, {cbor_move(cbor_build_string("t")), cbor_move(cbor_build_float8(timestamp))});

  cbor_map_add(root, {cbor_move(cbor_build_string("dt")),
                      cbor_move(cbor_build_float8(departure_timestamp))});

  cbor_map_add(root,
               {cbor_move(cbor_build_string("c")), cbor_move(cbor_build_uint8(chunk))});

  cbor_map_add(root,
               {cbor_move(cbor_build_string("l")), cbor_move(cbor_build_uint8(chunks))});

  if (key_frame) {
    cbor_map_add(
        root, {cbor_move(cbor_build_string("k")), cbor_move(cbor_build_bool(key_frame))});
  }

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

std::vector<network_frame> encoded_frame::to_network() const {
  std::vector<network_frame> frames;

  std::string encoded = std::move(satori::video::encode64(data));
  size_t chunks = std::ceil((double)encoded.length() / max_payload_size);

  for (size_t i = 0; i < chunks; i++) {
    network_frame frame;
    frame.base64_data = encoded.substr(i * max_payload_size, max_payload_size);
    frame.id = id;
    frame.t = timestamp;
    frame.chunk = static_cast<uint32_t>(i + 1);
    frame.chunks = static_cast<uint32_t>(chunks);
    frame.key_frame = key_frame;

    frames.push_back(std::move(frame));
  }

  return frames;
}

network_metadata parse_network_metadata(cbor_item_t *item) {
  cbor_incref(item);
  auto decref = gsl::finally([&item]() { cbor_decref(&item); });
  auto msg = cbor::map(item);

  const std::string name = msg.get_str("codecName");
  const std::string base64_data = msg.get_str("codecData");

  return network_metadata{name, base64_data};
}

network_frame parse_network_frame(cbor_item_t *item) {
  cbor_incref(item);
  auto decref = gsl::finally([&item]() { cbor_decref(&item); });
  auto msg = cbor::map(item);

  auto id = msg.get("i");
  int64_t i1 = cbor::get_int64(cbor_array_handle(id)[0]);
  int64_t i2 = cbor::get_int64(cbor_array_handle(id)[1]);

  std::chrono::high_resolution_clock::time_point timestamp;
  const cbor_item_t *t = msg.get("t");
  if (t != nullptr) {
    std::chrono::duration<double> double_duration(cbor::get_double(t));
    auto duration =
        std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(
            double_duration);
    timestamp = std::chrono::high_resolution_clock::time_point{duration};
  } else {
    LOG(WARNING) << "network frame packet doesn't have timestamp";
    timestamp = std::chrono::high_resolution_clock::now();
  }

  uint32_t chunk = 1, chunks = 1;
  const cbor_item_t *c = msg.get("c");
  if (c != nullptr) {
    const cbor_item_t *l = msg.get("l");
    CHECK(!cbor_isa_negint(c));
    CHECK(!cbor_isa_negint(l));
    CHECK_LE(cbor_int_get_width(c), CBOR_INT_32);
    CHECK_LE(cbor_int_get_width(l), CBOR_INT_32);
    chunk = static_cast<uint32_t>(cbor_get_int(c));
    chunks = static_cast<uint32_t>(cbor_get_int(l));
  }

  const cbor_item_t *k = msg.get("k");
  bool key_frame = false;
  if (k != nullptr) {
    key_frame = cbor_is_bool(k) && cbor_ctrl_is_bool(k);
  }

  network_frame frame;
  frame.base64_data = msg.get_str("d");
  frame.id = {i1, i2};
  frame.t = timestamp;
  frame.chunk = chunk;
  frame.chunks = chunks;
  frame.key_frame = key_frame;

  return frame;
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