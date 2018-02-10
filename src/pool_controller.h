#pragma once

#include <json.hpp>
#include <list>
#include <string>
#include "rtm_client.h"

namespace satori {
namespace video {

struct job_controller {
  virtual ~job_controller() = default;
  virtual void add_job(const nlohmann::json &job) = 0;
  virtual void remove_job(const nlohmann::json &job) = 0;
  virtual nlohmann::json list_jobs() const = 0;
};

class pool_job_controller : rtm::subscription_callbacks {
 public:
  pool_job_controller(boost::asio::io_service &io, const std::string &pool,
                      const std::string &job_type, size_t max_streams_capacity,
                      std::shared_ptr<rtm::client> &rtm_client, job_controller &streams);

  void start();
  void shutdown();

 private:
  void on_heartbeat(const boost::system::error_code &ec);
  void on_data(const rtm::subscription & /*subscription*/,
               rtm::channel_data &&data) override;
  void start_job(const nlohmann::json &job);
  void stop_job(const nlohmann::json &job);
  void on_error(std::error_condition ec) override;

  boost::asio::io_service &_io;
  const size_t _max_streams_capacity;
  const std::string _pool;
  const std::string _job_type;
  std::shared_ptr<rtm::client> _client;
  const rtm::subscription _pool_sub{};
  std::unique_ptr<boost::asio::deadline_timer> _hb_timer;
  job_controller &_streams;
};
}  // namespace video
}  // namespace satori
