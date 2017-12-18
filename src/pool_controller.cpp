#include "pool_controller.h"
#include <gsl/gsl>
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
                                         job_controller &&streams)
    : _io(io),
      _pool(pool),
      _job_type(job_type),
      _client(rtm_client),
      _streams(streams),
      _max_streams_capacity(max_streams_capacity) {}

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

  auto stream_list = _streams.list_jobs();
  cbor_item_t *jobs = cbor_new_definite_array(stream_list.size());
  for (const std::string &s : stream_list) {
    cbor_array_push(jobs, cbor_move(cbor_build_string(s.c_str())));
  }

  cbor_item_t *hb_message = cbor_new_indefinite_map();
  cbor_map_add(hb_message, {cbor_move(cbor_build_string("from")),
                            cbor_build_string(node_id.c_str())});
  cbor_map_add(hb_message,
               {cbor_move(cbor_build_string("active_jobs")), cbor_move(jobs)});

  cbor_item_t *available_capacity = cbor_new_indefinite_map();
  cbor_map_add(available_capacity,
               {cbor_move(cbor_build_string(_job_type.c_str())),
                cbor_build_uint64(_max_streams_capacity - stream_list.size())});

  cbor_map_add(hb_message, {cbor_move(cbor_build_string("available_capacity")),
                            cbor_move(available_capacity)});

  LOG(2) << "sending heartbeat: " << hb_message;
  _client->publish(_pool, cbor_move(hb_message));
}

void pool_job_controller::on_data(const rtm::subscription & /*subscription*/,
                                  rtm::channel_data &&data) {
  cbor_item_t *item = data.payload;
  cbor_incref(item);
  auto decref = gsl::finally([item]() mutable { cbor_decref(&item); });

  auto msg = cbor::map(item);
  if (msg.get_str("to") != node_id) {
    return;
  }

  if (cbor_item_t *start_job_msg = msg.get("start_job")) {
    start_job(start_job_msg);
  } else if (cbor_item_t *stop_job_msg = msg.get("stop_job")) {
    stop_job(stop_job_msg);
  } else {
    LOG(ERROR) << "unknown command: " << item;
  }
}

void pool_job_controller::start_job(cbor_item_t *msg) {
  LOG(INFO) << "start_job: " << msg;
  _streams.add_job(msg);
}

void pool_job_controller::stop_job(cbor_item_t *msg) {
  LOG(INFO) << "stop_job: " << msg;
  _streams.remove_job(msg);
}

void pool_job_controller::on_error(std::error_condition ec) {
  LOG(ERROR) << "rtm error: " << ec.message();
}

}  // namespace video
}  // namespace satori
