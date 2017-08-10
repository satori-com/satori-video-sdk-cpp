#pragma once

#include <rapidjson/document.h>
#include <memory>

#include "librtmvideo/data.h"
#include "librtmvideo/decoder.h"
#include "sink.h"
#include "source.h"

namespace rtm {
namespace video {

struct flow_json_decoder : public sink<rapidjson::Value, rapidjson::Value>,
                           public source<metadata, encoded_frame> {
 public:
  int init() override;
  void start() override;
  void on_metadata(rapidjson::Value &&m) override;
  void on_frame(rapidjson::Value &&f) override;
  bool empty() override;

 private:
  metadata decode_metadata_frame(const rapidjson::Value &msg);
  network_frame decode_network_frame(const rapidjson::Value &msg);
  void process_frame_part(frame_id &id, const uint8_t *data, size_t length,
                          uint32_t chunk, uint32_t chunks);
  void process_frame(frame_id &id, const uint8_t *data, size_t length);
  void on_metadata(metadata &&m);
  void on_frame(network_frame &&f);

  std::vector<uint8_t> _chunk_buffer;
  metadata _metadata;
};

}  // namespace video
}  // namespace rtm
