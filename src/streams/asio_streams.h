// streams working together with boost::asio
#pragma include once

#include <boost/asio.hpp>
#include <chrono>

#include "streams.h"

namespace satori {
namespace video {

namespace streams {
namespace asio {

// Stream transformation, that delays every item by custom time.
// Fn: std::chrono::duration(const T& t)
template <typename Fn>
inline auto delay(boost::asio::io_service &io, Fn &&fn);

// Periodically drains pipeline.
template <typename T>
streams::op<T, T> interval(boost::asio::io_service &io, std::chrono::milliseconds period);

// Breaks the stream after specified time.
template <typename T>
streams::op<T, T> timer_breaker(boost::asio::io_service &io,
                                std::chrono::milliseconds time);

}  // namespace asio
}  // namespace streams
}  // namespace video
}  // namespace satori
#include "asio_streams_impl.h"