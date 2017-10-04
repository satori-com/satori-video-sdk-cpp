#pragma once

#include <cbor.h>

#include "rtmclient.h"
#include "streams.h"

namespace streams {
namespace rtm {

streams::publisher<cbor_item_t *> cbor_channel(
    std::shared_ptr<::rtm::subscriber> subscriber, const std::string &channel,
    const ::rtm::subscription_options &options);

streams::subscriber<cbor_item_t *> &cbor_sink(std::shared_ptr<::rtm::publisher> client,
                                              const std::string &channel);

}  // namespace rtm
}  // namespace streams
