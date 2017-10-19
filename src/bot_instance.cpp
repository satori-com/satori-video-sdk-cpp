#include "bot_instance.h"

#include <vector>

#include "cbor_tools.h"
#include "satorivideo/tele.h"
#include "stopwatch.h"

namespace satori {
namespace video {
namespace {
auto processing_times_millis = tele::distribution_new("vbot", "processing_times_millis");
auto frames_processed = tele::counter_new("vbot", "frames_processed");
auto messages_sent = tele::counter_new("vbot", "messages_sent");
auto analysis_messages_sent = tele::counter_new("vbot", "analysis.messages_sent");
auto debug_messages_sent = tele::counter_new("vbot", "debug.messages_sent");
auto control_messages_sent = tele::counter_new("vbot", "control.messages_sent");
auto control_messages_received = tele::counter_new("vbot", "control.messages_received");

}  // namespace

bot_instance::bot_instance(const std::string& bot_id, const execution_mode execmode,
                           const bot_descriptor& descriptor)
    : _bot_id(bot_id),
      _descriptor(descriptor),
      bot_context{nullptr, &_image_metadata, execmode} {}

bot_instance::~bot_instance() {}

void bot_instance::queue_message(const bot_message_kind kind, cbor_item_t* message,
                                 const frame_id& id) {
  struct bot_message newmsg {
    message, kind, id
  };
  cbor_incref(message);
  _message_buffer.push_back(newmsg);
}

void bot_instance::process_image_metadata(const owned_image_metadata& metadata) {}

void bot_instance::process_image_frame(const owned_image_frame& frame) {
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
  tele::distribution_add(processing_times_millis, s.millis());
  tele::counter_inc(frames_processed);

  prepare_messages_for_sending(frame.id);
}

void bot_instance::process_control_message(cbor_item_t* msg) {
  tele::counter_inc(control_messages_received);
  auto cbor_deleter = gsl::finally([&msg]() { cbor_decref(&msg); });

  if (cbor_isa_array(msg)) {
    for (int i = 0; i < cbor_array_size(msg); ++i) {
      process_control_message(cbor_array_get(msg, i));
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

  prepare_messages_for_sending(frame_id{-1, -1});
}

void bot_instance::prepare_messages_for_sending(const frame_id& id) {
  tele::counter_inc(messages_sent, _message_buffer.size());

  for (auto&& msg : _message_buffer) {
    switch (msg.kind) {
      case bot_message_kind::ANALYSIS:
        tele::counter_inc(analysis_messages_sent);
        break;
      case bot_message_kind::DEBUG:
        tele::counter_inc(debug_messages_sent);
        break;
      case bot_message_kind::CONTROL:
        tele::counter_inc(control_messages_sent);
        break;
    }

    cbor_item_t* data = msg.data;
    CHECK(cbor_isa_map(data)) << "message data is not a map: " << data;

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
}

streams::op<bot_instance_input, bot_instance_output> bot_instance::process() {
  return [this](streams::publisher<bot_instance_input>&& src) {
    return std::move(src) >> streams::flat_map([this](bot_instance_input&& p) {
             auto message_buffer_cleaner =
                 gsl::finally([this]() { _message_buffer.clear(); });

             if (const owned_image_metadata* m = boost::get<owned_image_metadata>(&p)) {
               process_image_metadata(*m);

               return streams::publishers::concat<bot_instance_output>(
                   streams::publishers::of<bot_instance_output>({*m}),
                   streams::publishers::of<bot_instance_output>(
                       std::vector<bot_instance_output>{
                           std::make_move_iterator(std::begin(_message_buffer)),
                           std::make_move_iterator(std::end(_message_buffer))}));

             } else if (const owned_image_frame* f = boost::get<owned_image_frame>(&p)) {
               process_image_frame(*f);

               return streams::publishers::concat<bot_instance_output>(
                   streams::publishers::of<bot_instance_output>({*f}),
                   streams::publishers::of<bot_instance_output>(
                       std::vector<bot_instance_output>{
                           std::make_move_iterator(std::begin(_message_buffer)),
                           std::make_move_iterator(std::end(_message_buffer))}));

             } else if (cbor_item_t** t = boost::get<cbor_item_t*>(&p)) {
               process_control_message(*t);

               return streams::publishers::of<bot_instance_output>(
                   std::vector<bot_instance_output>{
                       std::make_move_iterator(std::begin(_message_buffer)),
                       std::make_move_iterator(std::end(_message_buffer))});
             } else {
               ABORT() << "Bad variant";
             }
           });
  };
}

}  // namespace video
}  // namespace satori
