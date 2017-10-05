#pragma once

#include <cbor.h>

#include "rtm_client.h"
#include "streams/streams.h"

namespace satori {
namespace video {
namespace rtm {

streams::publisher<cbor_item_t *> cbor_channel(
    std::shared_ptr<rtm::subscriber> subscriber, const std::string &channel,
    const subscription_options &options);

streams::subscriber<cbor_item_t *> &cbor_sink(std::shared_ptr<publisher> client,
                                              const std::string &channel);

}  // namespace rtm
}  // namespace video
}  // namespace satori
