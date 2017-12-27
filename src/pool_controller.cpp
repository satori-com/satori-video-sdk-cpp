#include "pool_controller.h"
#include <gsl/gsl>
#include <json.hpp>
#include <random>

#include "cbor_tools.h"

namespace satori {
namespace video {
namespace {
const auto default_hb_period = boost::posix_time::seconds(1);
std::string make_node_id() {
  static std::default_random_engine init_node_id_rng_device(
      (unsigned)std::chrono::system_clock::now().time_since_epoch().count());
  std::default_random_engine rng_engine(init_node_id_rng_device());
  std::uniform_int_distribution<unsigned long long> node_id_gen;
  return std::to_string(node_id_gen(rng_engine));
}
const std::string node_id = make_node_id();
}  // namespace

pool_job_controller::pool_job_controller(boost::asio::io_service &io,
                                         const std::string &pool,
                                         const std::string &job_type,
                                         size_t max_streams_capacity,
                                         std::shared_ptr<rtm::client> &rtm_client,
                                         job_controller &streams)
    : _io(io),
      _max_streams_capacity(max_streams_capacity),
      _pool(pool),
      _job_type(job_type),
      _client(rtm_client),
      _streams(streams) {}

pool_job_controller::~pool_job_controller() {
  if (_hb_timer) {
    _client->unsubscribe(_pool_sub);
  }
}

void pool_job_controller::start() {
  LOG(INFO) << "joining pool " << _pool << " job_type=" << _job_type
            << " node_id=" << node_id;
  _client->subscribe_channel(_pool, _pool_sub, *this, nullptr);
  _hb_timer = std::make_unique<boost::asio::deadline_timer>(_io);
  on_heartbeat({});
}

void pool_job_controller::on_heartbeat(const boost::system::error_code &ec) {
  CHECK(!ec) << "boost error: [" << ec << "] " << ec.message();

  _hb_timer->expires_from_now(default_hb_period);
  _hb_timer->async_wait([this](const boost::system::error_code &e) { on_heartbeat(e); });

  const auto jobs = _streams.list_jobs();
  CHECK(jobs.is_array()) << "not an array: " << jobs;

  nlohmann::json available_capacity = nlohmann::json::object();
  available_capacity[_job_type] = _max_streams_capacity - jobs.size();

  nlohmann::json hb_message = nlohmann::json::object();
  hb_message["from"] = node_id;
  hb_message["active_jobs"] = jobs;
  hb_message["available_capacity"] = available_capacity;

  LOG(2) << "sending heartbeat: " << hb_message;
  _client->publish(_pool, std::move(hb_message));
}

void pool_job_controller::shutdown() {
  nlohmann::json shutdown_note = nlohmann::json::object();
  shutdown_note["from"] = node_id;
  shutdown_note["job_type"] = _job_type;
  shutdown_note["reason"] = "shutdown";
  shutdown_note["stopped_jobs"] = _streams.list_jobs();

  _io.post([
    client = _client, pool = _pool, shutdown_note = std::move(shutdown_note)
  ]() mutable { client->publish(pool, std::move(shutdown_note)); });
}

void pool_job_controller::on_data(const rtm::subscription & /*subscription*/,
                                  rtm::channel_data &&data) {
  auto &msg = data.payload;
  if (msg.find("to") == msg.end() || msg["to"] != node_id) {
    return;
  }

  if (msg.find("start_job") != msg.end()) {
    start_job(msg["start_job"]);
  } else if (msg.find("stob_job") != msg.end()) {
    stop_job(msg["stop_job"]);
  } else {
    LOG(ERROR) << "unknown command: " << msg;
  }
}

void pool_job_controller::start_job(const nlohmann::json &job) {
  LOG(INFO) << "start_job: " << job;
  _streams.add_job(job);
}

void pool_job_controller::stop_job(const nlohmann::json &job) {
  LOG(INFO) << "stop_job: " << job;
  _streams.remove_job(job);
}

void pool_job_controller::on_error(std::error_condition ec) {
  LOG(ERROR) << "rtm error: " << ec.message();
}

}  // namespace video
}  // namespace satori
