#pragma once

namespace rtm {
namespace video {

struct bot_message {
  cbor_item_t *data;
  bot_message_kind kind;
};

class bot_environment : public subscription_callbacks {
 public:
  static bot_environment& instance();

  void register_bot(const bot_descriptor* bot);
  int main(int argc, char* argv[]);
  void on_error(error e, const std::string& msg) override;
  void on_data(const subscription& sub,
               const rapidjson::Value& value) override;

  void on_metadata(const rapidjson::Value& msg);
  void on_frame_data(const rapidjson::Value& msg);
  void send_message(bot_message message);
  void send_messages();
  void store_bot_message(const bot_message_kind kind, cbor_item_t *message);

 private:
  const bot_descriptor* _bot{nullptr};
  bot_context* _ctx{nullptr};
  rtm::subscription _frames_subscription;
  rtm::subscription _metadata_subscription;
  std::string _channel;
  decoder* _decoder{nullptr};
  std::unique_ptr<rtm::client> _client;
};

}
}
