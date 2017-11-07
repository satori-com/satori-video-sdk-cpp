#include "bot_instance.h"

#include "cbor_tools.h"
#include "metrics.h"
#include "stopwatch.h"

namespace satori {
namespace video {
namespace {
auto& processing_times_millis =
    prometheus::BuildHistogram()
        .Name("frame_processing_times_millis")
        .Register(metrics_registry())
        .Add({}, std::vector<double>{0,  1,  2,  5,  10,  15,  20,  25,  30,  40, 50,
                                     60, 70, 80, 90, 100, 200, 300, 400, 500, 750});
auto& frames_processed = prometheus::BuildCounter()
                             .Name("frames_processed")
                             .Register(metrics_registry())
                             .Add({});
auto& messages_sent =
    prometheus::BuildCounter().Name("messages_sent").Register(metrics_registry());
auto& messages_received =
    prometheus::BuildCounter().Name("messages_received").Register(metrics_registry());

}  // namespace

struct bot_instance::control_sub : public streams::subscriber<cbor_item_t*> {
  explicit control_sub(bot_instance* const bot) : _bot(bot) {}

  ~control_sub() override {
    if (_video_sub != nullptr) {
      _video_sub->cancel();
    }
  }

  void on_next(cbor_item_t*&& t) override {
    _bot->operator()(t);
    _video_sub->request(1);
  }

  void on_error(std::error_condition ec) override {
    ABORT() << "Error in control stream: " << ec.message();
  }

  void on_complete() override { _video_sub = nullptr; }

  void on_subscribe(streams::subscription& s) override {
    _video_sub = &s;
    _video_sub->request(1);
  }

  bot_instance* const _bot;
  streams::subscription* _video_sub{nullptr};
};

bot_instance::bot_instance(const std::string& bot_id, const execution_mode execmode,
                           const bot_descriptor& descriptor,
                           satori::video::bot_environment& env)
    : _bot_id(bot_id),
      _descriptor(descriptor),
      _env(env),
      bot_context{nullptr, &_image_metadata, execmode,
                  satori::video::metrics_registry()} {}

bot_instance::~bot_instance() = default;

void bot_instance::start(streams::publisher<owned_image_packet>& video_stream,
                         streams::publisher<cbor_item_t*>& control_stream) {
  _control_sub = std::make_unique<control_sub>(this);
  control_stream->subscribe(*_control_sub);
  // this will drain the stream in batch mode, should be last.
  video_stream->subscribe(*this);
}

void bot_instance::stop() {
  if (_video_sub != nullptr) {
    _video_sub->cancel();
    _video_sub = nullptr;
  }
  _control_sub.reset();
}

void bot_instance::on_error(std::error_condition ec) { ABORT() << ec.message(); }

void bot_instance::on_next(owned_image_packet&& packet) {
  boost::apply_visitor(*this, packet);
  _video_sub->request(1);
}

void bot_instance::on_complete() {
  LOG(INFO) << "processing complete";
  _video_sub = nullptr;
}

void bot_instance::on_subscribe(streams::subscription& s) {
  _video_sub = &s;
  _video_sub->request(1);
}

void bot_instance::queue_message(const bot_message_kind kind, cbor_item_t* message,
                                 const frame_id& id) {
  struct bot_message newmsg {
    message, kind, id
  };
  cbor_incref(message);
  _message_buffer.push_back(newmsg);
}

void bot_instance::operator()(const owned_image_metadata& /*metadata*/) {}

void bot_instance::operator()(const owned_image_frame& frame) {
  LOG(1) << "process_image_frame " << frame.width << "x" << frame.height;
  stopwatch<> s;

  if (frame.width != _image_metadata.width) {
    _image_metadata.width = frame.width;
    _image_metadata.height = frame.height;
    std::copy(frame.plane_strides, frame.plane_strides + MAX_IMAGE_PLANES,
              _image_metadata.plane_strides);
  }

  image_frame bframe;
  bframe.id = frame.id;
  for (int i = 0; i < MAX_IMAGE_PLANES; ++i) {
    if (frame.plane_data[i].empty()) {
      bframe.plane_data[i] = nullptr;
    } else {
      bframe.plane_data[i] = (const uint8_t*)frame.plane_data[i].data();
    }
  }

  _descriptor.img_callback(*this, bframe);
  processing_times_millis.Observe(s.millis());
  frames_processed.Increment();

  send_messages(frame.id);
}

void bot_instance::operator()(cbor_item_t* msg) {
  // todo(mike): https://github.com/jupp0r/prometheus-cpp/issues/75
//  messages_received.Add({{"message_type", "control"}}).Increment();
  cbor_incref(msg);
  auto msg_decref = gsl::finally([&msg]() { cbor_decref(&msg); });

  if (cbor_isa_array(msg)) {
    for (int i = 0; i < cbor_array_size(msg); ++i) {
      this->operator()(cbor_array_get(msg, i));
    }
    return;
  }

  if (!cbor_isa_map(msg)) {
    LOG(ERROR) << "unsupported kind of message: " << msg;
    return;
  }

  if (_bot_id.empty() || cbor::map(msg).get_str("to", "") != _bot_id) {
    return;
  }

  cbor_item_t* response = _descriptor.ctrl_callback(*this, msg);

  if (response != nullptr) {
    CHECK(cbor_isa_map(response)) << "bot response is not a map: " << response;
    cbor_item_t* request_id = cbor::map(msg).get("request_id");
    if (request_id != nullptr) {
      cbor_map_add(response, {cbor_move(cbor_build_string("request_id")),
                              cbor_move(cbor_copy(request_id))});
    }

    queue_message(bot_message_kind::CONTROL, cbor_move(response), frame_id{0, 0});
  }

  send_messages(frame_id{-1, -1});
}

void bot_instance::send_messages(const frame_id& id) {
  for (auto&& msg : _message_buffer) {
    // todo(mike): https://github.com/jupp0r/prometheus-cpp/issues/75
/*
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
*/

    cbor_item_t* data = msg.data;

    int64_t ei1 = msg.id.i1 == 0 ? id.i1 : msg.id.i1;
    int64_t ei2 = msg.id.i2 == 0 ? id.i1 : msg.id.i2;

    if (ei1 >= 0) {
      cbor_item_t* is = cbor_new_definite_array(2);
      cbor_array_set(is, 0, cbor_move(cbor_build_uint64(static_cast<uint64_t>(ei1))));
      cbor_array_set(is, 1, cbor_move(cbor_build_uint64(static_cast<uint64_t>(ei2))));
      cbor_map_add(data, {cbor_move(cbor_build_string("i")), cbor_move(is)});
    }

    if (!_bot_id.empty()) {
      cbor_map_add(data, {cbor_move(cbor_build_string("from")),
                          cbor_move(cbor_build_string(_bot_id.c_str()))});
    }
  }
  _env.send_messages(std::move(_message_buffer));
}

}  // namespace video
}  // namespace satori
