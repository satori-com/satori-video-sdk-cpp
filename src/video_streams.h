#pragma once

#include <boost/asio.hpp>
#include <functional>
#include <memory>

#include "librtmvideo/data.h"
#include "rtmclient.h"
#include "streams.h"

namespace rtm {
namespace video {

void initialize_source_library();

streams::publisher<encoded_packet> file_source(boost::asio::io_service &io,
                                               std::string filename, bool loop,
                                               bool synchronous);

streams::publisher<encoded_packet> camera_source(boost::asio::io_service &io,
                                                 const std::string &dimensions);

streams::publisher<network_packet> network_replay_source(boost::asio::io_service &io,
                                                         const std::string &filename,
                                                         bool synchronous);

streams::publisher<network_packet> rtm_source(std::shared_ptr<rtm::subscriber> client,
                                              const std::string &channel_name);

streams::op<network_packet, encoded_packet> decode_network_stream();

streams::op<encoded_packet, image_frame> decode_image_frames(
    int bounding_width, int bounding_height, image_pixel_format pixel_format);

}  // namespace video
}  // namespace rtm
