#include "bot_instance.h"

#include <gsl/gsl>

#include "cbor_tools.h"
#include "metrics.h"
#include "stopwatch.h"

namespace satori {
namespace video {
namespace {
auto& processing_times_millis =
    prometheus::BuildHistogram()
        .Name("frame_batch_processing_times_millis")
        .Register(metrics_registry())
        .Add({}, std::vector<double>{0,  1,  2,  5,  10,  15,  20,  25,  30,  40, 50,
                                     60, 70, 80, 90, 100, 200, 300, 400, 500, 750});
auto& frame_size =
    prometheus::BuildHistogram()
        .Name("frame_size")
        .Register(metrics_registry())
        .Add({}, std::vector<double>{0,  1,  2,  3,   4,   5,   6,   7,   8,
                                     9,  10, 15, 20,  25,  30,  40,  50,  60,
                                     70, 80, 90, 100, 200, 300, 400, 500, 750});

auto& frame_batch_processed_total = prometheus::BuildCounter()
                                        .Name("frame_batch_processed_total")
                                        .Register(metrics_registry())
                                        .Add({});
auto& messages_sent =
    prometheus::BuildCounter().Name("messages_sent").Register(metrics_registry());
auto& messages_received =
    prometheus::BuildCounter().Name("messages_received").Register(metrics_registry());

cbor_item_t* build_configure_command(cbor_item_t* config) {
  cbor_item_t* cmd = cbor_new_definite_map(2);
  cbor_map_add(cmd, {cbor_move(cbor_build_string("action")),
                     cbor_move(cbor_build_string("configure"))});
  cbor_map_add(cmd, {cbor_move(cbor_build_string("body")), config});
  return cmd;
}

cbor_item_t* build_shutdown_command() {
  cbor_item_t* cmd = cbor_new_definite_map(2);
  cbor_map_add(cmd, {cbor_move(cbor_build_string("action")),
                     cbor_move(cbor_build_string("shutdown"))});
  return cmd;
}

}  // namespace

bot_instance::bot_instance(const std::string& bot_id, const execution_mode execmode,
                           const multiframe_bot_descriptor& descriptor)
    : _bot_id(bot_id),
      _descriptor(descriptor),
      bot_context{nullptr,
                  &_image_metadata,
                  execmode,
                  {
                      satori::video::metrics_registry(),
                      prometheus::BuildCounter()
                          .Name("frames_processed_total")
                          .Register(satori::video::metrics_registry())
                          .Add({}),
                      prometheus::BuildCounter()
                          .Name("frames_dropped_total")
                          .Register(satori::video::metrics_registry())
                          .Add({}),
                      prometheus::BuildHistogram()
                          .Name("frame_processing_times_millis")
                          .Register(satori::video::metrics_registry())
                          .Add({}, std::vector<double>{0,  1,   2,   5,   10,  15,  20,
                                                       25, 30,  40,  50,  60,  70,  80,
                                                       90, 100, 200, 300, 400, 500, 750}),
                  }} {}

streams::op<bot_input, bot_output> bot_instance::run_bot() {
  return [this](streams::publisher<bot_input>&& src) {
    auto main_stream =
        std::move(src)
        >> streams::map([this](bot_input&& p) { return boost::apply_visitor(*this, p); })
        >> streams::flatten();

    // TODO: maybe initial config and shutdown message should be sent from the same place?
    auto shutdown_stream = streams::generators<bot_output>::stateful(
        [this]() {
          LOG(INFO) << "shutting down bot";
          if (_descriptor.ctrl_callback) {
            cbor_item_t* cmd = build_shutdown_command();
            CHECK_EQ(1, cbor_refcount(cmd));
            auto cbor_deleter = gsl::finally([&cmd]() { cbor_decref(&cmd); });
            cbor_item_t* response = _descriptor.ctrl_callback(*this, cmd);
            if (response != nullptr) {
              LOG(INFO) << "got shutdown response: " << response;
              queue_message(bot_message_kind::DEBUG, response, frame_id{0, 0});
            } else {
              LOG(INFO) << "shutdown response is null";
            }
          }

          prepare_message_buffer_for_downstream();

          return nullptr;
        },
        [this](void*, streams::observer<bot_output>& sink) {
          if (_message_buffer.empty()) {
            sink.on_complete();
            return;
          }

          LOG(INFO) << "sending shutdown";
          struct bot_message msg = std::move(_message_buffer.front());
          _message_buffer.pop_front();

          sink.on_next(std::move(msg));
        });

    return streams::publishers::concat(std::move(main_stream),
                                       std::move(shutdown_stream));
  };
}

void bot_instance::queue_message(const bot_message_kind kind, cbor_item_t* message,
                                 const frame_id& id) {
  cbor_incref(message);
  auto message_decref = gsl::finally([&message]() { cbor_decref(&message); });

  cbor_item_t* message_copy = cbor_copy(message);
  CHECK_EQ(1, cbor_refcount(message_copy));
  frame_id effective_frame_id =
      (id.i1 == 0 && id.i2 == 0 && _current_frame_id.i1 != 0 && _current_frame_id.i2 != 0)
          ? _current_frame_id
          : id;

  struct bot_message newmsg {
    cbor_move(message_copy), kind, effective_frame_id
  };
  _message_buffer.push_back(newmsg);
}

void bot_instance::set_current_frame_id(const frame_id& id) { _current_frame_id = id; }

std::vector<image_frame> bot_instance::extract_frames(
    const std::list<bot_output>& packets) {
  std::vector<image_frame> result;

  for (const auto& p : packets) {
    auto* frame = boost::get<owned_image_frame>(&p);

    if (frame == nullptr) {
      continue;
    }

    if (frame->width != _image_metadata.width) {
      _image_metadata.width = frame->width;
      _image_metadata.height = frame->height;
      std::copy(frame->plane_strides, frame->plane_strides + max_image_planes,
                _image_metadata.plane_strides);
    }

    image_frame bframe;
    bframe.id = frame->id;
    for (int i = 0; i < max_image_planes; ++i) {
      if (frame->plane_data[i].empty()) {
        bframe.plane_data[i] = nullptr;
      } else {
        bframe.plane_data[i] = (const uint8_t*)frame->plane_data[i].data();
      }
    }
    result.push_back(std::move(bframe));
  }
  return result;
}

std::list<bot_output> bot_instance::operator()(std::queue<owned_image_packet>& pp) {
  stopwatch<> s;
  std::list<bot_output> result;

  frame_size.Observe(pp.size());

  while (!pp.empty()) {
    result.emplace_back(pp.front());
    pp.pop();
  }

  std::vector<image_frame> bframes = extract_frames(result);

  if (!bframes.empty()) {
    LOG(1) << "process " << bframes.size() << " frames " << _image_metadata.width << "x"
           << _image_metadata.height;

    _descriptor.img_callback(*this, gsl::span<image_frame>(bframes));
    frame_batch_processed_total.Increment();

    prepare_message_buffer_for_downstream();

    std::copy(_message_buffer.begin(), _message_buffer.end(), std::back_inserter(result));
    _message_buffer.clear();
  }

  processing_times_millis.Observe(s.millis());
  return result;
}

std::list<bot_output> bot_instance::operator()(cbor_item_t* msg) {
  messages_received.Add({{"message_type", "control"}}).Increment();
  cbor_incref(msg);
  auto msg_decref = gsl::finally([&msg]() { cbor_decref(&msg); });

  if (cbor_isa_array(msg)) {
    std::list<bot_output> aggregated;
    for (size_t i = 0; i < cbor_array_size(msg); ++i) {
      aggregated.splice(aggregated.end(), this->operator()(cbor_array_get(msg, i)));
    }
    return aggregated;
  }

  if (!cbor_isa_map(msg)) {
    LOG(ERROR) << "unsupported kind of message: " << msg;
    return std::list<bot_output>{};
  }

  if (_bot_id.empty() || cbor::map(msg).get_str("to", "") != _bot_id) {
    return std::list<bot_output>{};
  }

  cbor_item_t* response = _descriptor.ctrl_callback(*this, msg);

  if (response != nullptr) {
    CHECK(cbor_isa_map(response)) << "bot response is not a map: " << response;

    cbor_item_t* request_id = cbor::map(msg).get("request_id");
    if (request_id != nullptr) {
      cbor_map_add(response, {cbor_move(cbor_build_string("request_id")),
                              cbor_move(cbor_copy(request_id))});
    }

    queue_message(bot_message_kind::CONTROL, response, frame_id{0, 0});
  }

  prepare_message_buffer_for_downstream();

  std::list<bot_output> result{_message_buffer.begin(), _message_buffer.end()};
  _message_buffer.clear();
  return result;
}

void bot_instance::prepare_message_buffer_for_downstream() {
  for (auto&& msg : _message_buffer) {
    switch (msg.kind) {
      case bot_message_kind::ANALYSIS:
        messages_sent.Add({{"message_type", "analysis"}}).Increment();
        break;
      case bot_message_kind::DEBUG:
        messages_sent.Add({{"message_type", "debug"}}).Increment();
        break;
      case bot_message_kind::CONTROL:
        messages_sent.Add({{"message_type", "control"}}).Increment();
        break;
    }

    cbor_item_t* data = msg.data;

    if (msg.id.i1 >= 0) {
      cbor_item_t* is = cbor_new_definite_array(2);
      cbor_array_set(is, 0,
                     cbor_move(cbor_build_uint64(static_cast<uint64_t>(msg.id.i1))));
      cbor_array_set(is, 1,
                     cbor_move(cbor_build_uint64(static_cast<uint64_t>(msg.id.i2))));
      cbor_map_add(data, {cbor_move(cbor_build_string("i")), cbor_move(is)});
    }

    if (!_bot_id.empty()) {
      cbor_map_add(data, {cbor_move(cbor_build_string("from")),
                          cbor_move(cbor_build_string(_bot_id.c_str()))});
    }
  }
}

void bot_instance::configure(cbor_item_t* config) {
  if (!_descriptor.ctrl_callback) {
    if (config == nullptr) {
      return;
    }
    ABORT() << "Bot control handler was not provided but config was";
  }

  if (config == nullptr) {
    LOG(INFO) << "using empty bot configuration";
    config = cbor_new_definite_map(0);
  } else {
    cbor_incref(config);
  }

  cbor_item_t* cmd = build_configure_command(config);
  CHECK_EQ(1, cbor_refcount(cmd));
  auto cbor_deleter = gsl::finally([&config, &cmd]() {
    cbor_decref(&config);
    cbor_decref(&cmd);
  });

  LOG(INFO) << "configuring bot: " << cmd;
  cbor_item_t* response = _descriptor.ctrl_callback(*this, cmd);
  if (response != nullptr) {
    queue_message(bot_message_kind::DEBUG, response, frame_id{0, 0});
  }
}

}  // namespace video
}  // namespace satori
