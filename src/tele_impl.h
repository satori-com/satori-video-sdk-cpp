#pragma once

#include <cbor.h>
#include <boost/asio.hpp>
#include "librtmvideo/tele.h"

#include "rtm_client.h"

namespace tele {

class publisher {
 public:
  publisher(rtm::publisher &rtm_publisher, boost::asio::io_service &io_service);
  ~publisher();

 private:
  boost::asio::deadline_timer _timer;
};

}  // namespace tele
