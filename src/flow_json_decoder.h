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
                           public source<network_metadata, network_frame> {
 public:
  int init() override;
  void start() override;
  void on_metadata(rapidjson::Value &&m) override;
  void on_frame(rapidjson::Value &&f) override;
  bool empty() override;

 private:
  network_metadata decode_metadata_frame(const rapidjson::Value &msg);
  network_frame decode_network_frame(const rapidjson::Value &msg);
};

}  // namespace video
}  // namespace rtm
