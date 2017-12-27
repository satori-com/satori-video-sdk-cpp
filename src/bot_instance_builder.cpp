#include "bot_instance_builder.h"
#include <cbor.h>

namespace satori {
namespace video {
bot_instance_builder::bot_instance_builder(multiframe_bot_descriptor descriptor)
    : _descriptor(std::move(descriptor)) {}

bot_instance_builder &bot_instance_builder::set_execution_mode(execution_mode mode) {
  _mode = mode;
  return *this;
}

bot_instance_builder &bot_instance_builder::set_config(const nlohmann::json &config) {
  _config = config;
  return *this;
}

bot_instance_builder &bot_instance_builder::set_bot_id(std::string id) {
  _id = std::move(id);
  return *this;
}

std::unique_ptr<bot_instance> bot_instance_builder::build() {
  auto instance = std::make_unique<bot_instance>(_id, _mode, _descriptor);
  instance->configure(_config);
  return instance;
}

}  // namespace video
}  // namespace satori
