#pragma once

#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <gsl/gsl>
#include <list>
#include <memory>

#include "bot_instance.h"
#include "data.h"
#include "satorivideo/video_bot.h"
#include "rtm_client.h"
#include "tele_impl.h"
#include "video_streams.h"

namespace satori {
namespace video {

class bot_environment : private rtm::error_callbacks {
 public:
  static bot_environment& instance();

  void register_bot(const bot_descriptor* bot);
  int main(int argc, char* argv[]);

  rtm::publisher& publisher() { return *_rtm_client; }

 private:
  void parse_config(boost::optional<std::string> config_file);
  void on_error(std::error_condition ec) override;
  void on_bot_message(struct bot_message&& msg);

  const bot_descriptor* _bot_descriptor{nullptr};
  std::shared_ptr<bot_instance> _bot_instance;
  std::shared_ptr<rtm::client> _rtm_client;
  std::unique_ptr<tele::publisher> _tele_publisher;

  streams::observer<cbor_item_t*>* _analysis_sink;
  streams::observer<cbor_item_t*>* _debug_sink;
  streams::observer<cbor_item_t*>* _control_sink;

  std::unique_ptr<std::ofstream> _analysis_file;
  std::unique_ptr<std::ofstream> _debug_file;

  streams::publisher<owned_image_packet> _source;
  streams::publisher<cbor_item_t*> _control_source;
};

}  // namespace video
}  // namespace satori
