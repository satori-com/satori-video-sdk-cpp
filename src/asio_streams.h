// streams working together with boost::asio
#pragma include once

#include <boost/asio.hpp>
#include <chrono>

#include "streams.h"

namespace streams {
namespace asio {

// Stream transformation, that delays every item by custom time.
// Fn: std::chrono::duration(const T& t)
template <typename Fn>
inline auto delay(boost::asio::io_service &io, Fn &&fn);

// Periodically drains pipeline.
template <typename T>
streams::op<T, T> interval(boost::asio::io_service &io, std::chrono::milliseconds period);

}  // namespace asio
}  // namespace streams

#include "asio_streams_impl.h"