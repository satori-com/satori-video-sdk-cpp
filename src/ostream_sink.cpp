#include "ostream_sink.h"

#include "cbor_tools.h"
#include "logging.h"

namespace satori {
namespace video {
namespace streams {

namespace {
class ostream_observer : public streams::observer<cbor_item_t *> {
 public:
  explicit ostream_observer(std::ostream &out) : _out(out) {}

 private:
  void on_next(cbor_item_t *&&t) override { _out << t << "\n"; }

  void on_error(std::error_condition ec) override {
    LOG(ERROR) << "ERROR: " << ec.message();
    _out.flush();
    delete this;
  }

  void on_complete() override {
    _out.flush();
    delete this;
  }

  std::ostream &_out;
};

}  // namespace

streams::observer<cbor_item_t *> &ostream_sink(std::ostream &out) {
  return *(new ostream_observer(out));
}

}  // namespace streams
}  // namespace video
}  // namespace satori