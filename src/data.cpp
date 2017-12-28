#include <cmath>
#include <gsl/gsl>

#include "base64.h"
#include "data.h"
#include "logging.h"

namespace satori {
namespace video {

namespace {

double time_point_to_value(std::chrono::system_clock::time_point p) {
  auto duration = p.time_since_epoch();
  auto seconds_duration =
      std::chrono::duration_cast<std::chrono::duration<double>>(duration);
  double timestamp = seconds_duration.count();
  return timestamp;
}

std::chrono::system_clock::time_point json_to_time_point(const nlohmann::json &item) {
  std::chrono::duration<double> double_duration(item.get<double>());
  auto duration =
      std::chrono::duration_cast<std::chrono::system_clock::duration>(double_duration);
  return std::chrono::system_clock::time_point{duration};
}

}  // namespace

nlohmann::json network_frame::to_json() const {
  nlohmann::json result = nlohmann::json::object();
  result["d"] = base64_data;
  result["i"] = {id.i1, id.i2};
  result["t"] = time_point_to_value(t);
  result["dt"] = time_point_to_value(std::chrono::system_clock::now());
  result["c"] = chunk;
  result["l"] = chunks;

  if (key_frame) {
    result["k"] = key_frame;
  }

  return result;
}

nlohmann::json network_metadata::to_json() const {
  nlohmann::json result = nlohmann::json::object();
  result["codecName"] = codec_name;
  result["codecData"] = base64_data;

  if (!additional_data.is_null()) {
    CHECK(additional_data.is_object()) << "not an object: " << additional_data;
    for (auto it = additional_data.begin(); it != additional_data.end(); ++it) {
      result[it.key()] = it.value();
    }
  }

  return result;
}

network_metadata encoded_metadata::to_network() const {
  network_metadata nm;

  nm.codec_name = codec_name;
  if (!codec_data.empty()) {
    nm.base64_data = std::move(satori::video::encode64(codec_data));
  }
  nm.additional_data = additional_data;

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

network_metadata parse_network_metadata(const nlohmann::json &item) {
  CHECK(item.find("codecName") != item.end()) << "bad item: " << item;
  CHECK(item.find("codecData") != item.end()) << "bad item: " << item;
  auto &name = item["codecName"];
  auto &base64_data = item["codecData"];
  CHECK(name.is_string()) << "bad item: " << item;
  CHECK(base64_data.is_string()) << "bad item: " << item;

  return network_metadata{name, base64_data};
}

network_frame parse_network_frame(const nlohmann::json &item) {
  CHECK(item.find("i") != item.end()) << "bad item: " << item;
  auto &id = item["i"];
  CHECK(id.is_array()) << "bad item: " << item;
  int64_t i1 = id[0];
  int64_t i2 = id[1];

  std::chrono::system_clock::time_point timestamp;
  if (item.find("t") != item.end()) {
    auto &t = item["t"];
    CHECK(t.is_number()) << "bad item: " << item;
    timestamp = json_to_time_point(t);
  } else {
    LOG(WARNING) << "network frame packet doesn't have timestamp";
    timestamp = std::chrono::system_clock::now();
  }

  std::chrono::system_clock::time_point departure_time;
  if (item.find("dt") != item.end()) {
    auto &dt = item["dt"];
    CHECK(dt.is_number()) << "bad item: " << item;
    departure_time = json_to_time_point(dt);
  } else {
    LOG(WARNING) << "network frame packet doesn't have departure time";
    departure_time = std::chrono::system_clock::now();
  }

  uint32_t chunk = 1, chunks = 1;
  if (item.find("c") != item.end()) {
    CHECK(item.find("l") != item.end());
    auto &c = item["c"];
    auto &l = item["l"];
    CHECK(c.is_number_unsigned()) << "bad item: " << item;
    CHECK(l.is_number_unsigned()) << "bad item: " << item;
    chunk = c;
    chunks = l;
  }

  bool key_frame = false;
  if (item.find("k") != item.end()) {
    auto &k = item["k"];
    CHECK(k.is_boolean()) << "bad item: " << item;
    key_frame = k;
  }

  CHECK(item.find("d") != item.end()) << "bad item: " << item;
  auto &d = item["d"];
  CHECK(d.is_string()) << "bad item: " << item;

  network_frame frame;
  frame.base64_data = d;
  frame.id = {i1, i2};
  frame.t = timestamp;
  frame.dt = departure_time;
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