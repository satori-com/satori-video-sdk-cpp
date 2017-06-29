#pragma once

#include <cbor.h>
#include <boost/asio.hpp>
#include "librtmvideo/tele.h"

#include "rtmclient.h"

namespace tele {

class publisher {
 public:
  publisher(rtm::publisher &rtm_publisher, boost::asio::io_service &io_service);

 private:
  boost::asio::deadline_timer _timer;
};

}  // namespace tele
