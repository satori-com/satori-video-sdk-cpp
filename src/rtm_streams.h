#pragma once

#include <cbor.h>
#include <boost/asio.hpp>
#include <json.hpp>

#include "rtm_client.h"
#include "streams/streams.h"

namespace satori {
namespace video {
namespace rtm {

streams::publisher<channel_data> channel(
    const std::shared_ptr<rtm::subscriber> &subscriber, const std::string &channel,
    const subscription_options &options);

streams::subscriber<nlohmann::json> &sink(const std::shared_ptr<publisher> &client,
                                          boost::asio::io_service &io_service,
                                          const std::string &channel);

}  // namespace rtm
}  // namespace video
}  // namespace satori
