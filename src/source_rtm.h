#pragma once

#include <rapidjson/document.h>
#include <memory>
#include <string>

#include "frame_aggregator.h"
#include "librtmvideo/data.h"
#include "librtmvideo/rtmpacket.h"
#include "rtmclient.h"
#include "source.h"

namespace rtm {
namespace video {

struct rtm_source : public source<metadata, encoded_frame>,
                    private rtm::subscription_callbacks {
 public:
  rtm_source(std::shared_ptr<rtm::subscriber> client,
             const std::string &channel_name);
  ~rtm_source();

  int init() override;
  void start() override;

 private:
  void on_data(const subscription &subscription,
               const rapidjson::Value &value) override;

  void on_metadata(const rapidjson::Value &msg);
  void on_frame_data(const rapidjson::Value &msg);

  const std::shared_ptr<rtm::subscriber> _subscriber;
  const std::string _metadata_channel;
  const std::string _frames_channel;
  rtm::subscription _metadata_subscription;
  rtm::subscription _frames_subscription;
  frame_aggregator _aggregator;
};

}  // namespace video
}  // namespace rtm
