#include "bot_instance.h"

#include "librtmvideo/cbor_tools.h"
#include "stopwatch.h"

namespace rtm {
namespace video {
namespace {
auto processing_times_millis = tele::distribution_new("vbot", "processing_times_millis");
auto frames_processed = tele::counter_new("vbot", "frames_processed");
}  // namespace

struct bot_instance::control_sub : public streams::subscriber<cbor_item_t*> {
  explicit control_sub(bot_instance* const bot) : _bot(bot) {}

  ~control_sub() {
    if (_video_sub) _video_sub->cancel();
  }

  void on_next(cbor_item_t*&& t) override {
    _bot->process_control_message(t);
    _video_sub->request(1);
  }

  void on_error(std::error_condition ec) override {
    LOG_S(ERROR) << "Error in control stream: " << ec.message();
    _video_sub = nullptr;
    exit(1);
  }

  void on_complete() override { _video_sub = nullptr; }

  void on_subscribe(streams::subscription& s) override {
    _video_sub = &s;
    _video_sub->request(1);
  }

  bot_instance* const _bot;
  streams::subscription* _video_sub{nullptr};
};

bot_instance::bot_instance(const std::string& bot_id, const bot_descriptor& descriptor,
                           rtm::video::bot_environment& env)
    : _bot_id(bot_id), _descriptor(descriptor), _env(env) {
  frame_metadata = &_image_metadata;
}

bot_instance::~bot_instance() {}

void bot_instance::start(streams::publisher<owned_image_packet>& video_stream,
                         streams::publisher<cbor_item_t*>& control_stream) {
  _control_sub.reset(new control_sub(this));
  control_stream->subscribe(*_control_sub);
  // this will drain the stream in batch mode, should be last.
  video_stream->subscribe(*this);
}

void bot_instance::stop() {
  if (_video_sub) {
    _video_sub->cancel();
    _video_sub = nullptr;
  }
  _control_sub.reset();
}

void bot_instance::on_error(std::error_condition ec) {
  LOG_S(ERROR) << ec.message();
  _video_sub = nullptr;
  exit(2);
}

void bot_instance::on_next(owned_image_packet&& packet) {
  if (const owned_image_metadata* m = boost::get<owned_image_metadata>(&packet)) {
    process_image_metadata(*m);
  } else if (const owned_image_frame* f = boost::get<owned_image_frame>(&packet)) {
    process_image_frame(*f);
  } else {
    BOOST_ASSERT_MSG(false, "Bad variant");
  }
  _video_sub->request(1);
}

void bot_instance::on_complete() {
  LOG_S(INFO) << "processing complete";
  _video_sub = nullptr;
}

void bot_instance::on_subscribe(streams::subscription& s) {
  _video_sub = &s;
  _video_sub->request(1);
}

void bot_instance::queue_message(const bot_message_kind kind, cbor_item_t* message,
                                 const frame_id& id) {
  bot_message newmsg{message, kind, id};
  cbor_incref(message);
  _message_buffer.push_back(newmsg);
}

void bot_instance::process_image_metadata(const owned_image_metadata& metadata) {}

void bot_instance::process_image_frame(const owned_image_frame& frame) {
  LOG_S(1) << "process_image_frame " << frame.width << "x" << frame.height;
  stopwatch<> s;

  if (frame.width != _image_metadata.width) {
    _image_metadata.width = frame.width;
    _image_metadata.height = frame.height;
    std::copy(frame.plane_strides, frame.plane_strides + MAX_IMAGE_PLANES,
              _image_metadata.plane_strides);
  }
  image_frame bframe{.id = frame.id};

  for (int i = 0; i < MAX_IMAGE_PLANES; ++i) {
    if (frame.plane_data[i].empty()) {
      bframe.plane_data[i] = nullptr;
    } else {
      bframe.plane_data[i] = (const uint8_t*)frame.plane_data[i].data();
    }
  }

  _descriptor.img_callback(*this, bframe);
  tele::distribution_add(processing_times_millis, s.millis());
  tele::counter_inc(frames_processed);

  send_messages(frame.id);
}

void bot_instance::process_control_message(cbor_item_t* msg) {
  auto cbor_deleter = gsl::finally([&msg]() { cbor_decref(&msg); });

  if (cbor_isa_array(msg)) {
    for (int i = 0; i < cbor_array_size(msg); ++i) {
      process_control_message(cbor_array_get(msg, i));
    }
    return;
  }

  if (!cbor_isa_map(msg)) {
    LOG_S(ERROR) << "unsupported kind of message";
    return;
  }

  if (_bot_id.empty() || cbor::map_get_str(msg, "to", "") != _bot_id) {
    return;
  }

  cbor_item_t* response = _descriptor.ctrl_callback(*this, msg);

  if (response != nullptr) {
    BOOST_ASSERT(cbor_isa_map(response));
    cbor_item_t* request_id = cbor::map_get(msg, "request_id");
    if (request_id != nullptr) {
      cbor_map_add(response,
                   (struct cbor_pair){.key = cbor_move(cbor_build_string("request_id")),
                                      .value = cbor_move(cbor_copy(request_id))});
    }

    queue_message(bot_message_kind::CONTROL, cbor_move(response), frame_id{0, 0});
  }

  cbor_decref(&msg);
  send_messages({.i1 = -1, .i2 = -1});
}

void bot_instance::send_messages(const frame_id& id) {
  for (auto&& msg : _message_buffer) {
    cbor_item_t* data = msg.data;

    int64_t ei1 = msg.id.i1 == 0 ? id.i1 : msg.id.i1;
    int64_t ei2 = msg.id.i2 == 0 ? id.i1 : msg.id.i2;

    if (ei1 >= 0) {
      cbor_item_t* is = cbor_new_definite_array(2);
      cbor_array_set(is, 0, cbor_move(cbor_build_uint64(static_cast<uint64_t>(ei1))));
      cbor_array_set(is, 1, cbor_move(cbor_build_uint64(static_cast<uint64_t>(ei2))));
      cbor_map_add(data,
                   {.key = cbor_move(cbor_build_string("i")), .value = cbor_move(is)});
    }

    if (!_bot_id.empty()) {
      cbor_map_add(data, {.key = cbor_move(cbor_build_string("from")),
                          .value = cbor_move(cbor_build_string(_bot_id.c_str()))});
    }
  }
  _env.send_messages(std::move(_message_buffer));
}

}  // namespace video
}  // namespace rtm