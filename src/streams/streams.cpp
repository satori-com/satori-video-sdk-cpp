#include "streams.h"

#include <fstream>

namespace satori {
namespace video {

namespace streams {
publisher<std::string> read_lines(const std::string &filename) {
  struct state {
    std::ifstream _input;

    void generate(int count, streams::observer<std::string> &observer) {
      for (int sent = 0; sent < count; ++sent) {
        std::string line;
        if (!std::getline(_input, line)) {
          observer.on_complete();
          return;
        }
        observer.on_next(std::move(line));
      }
    }
  };

  return streams::generators<std::string>::stateful(
      [filename]() { return new state{std::ifstream(filename)}; },
      [](state *s, int count, streams::observer<std::string> &sink) {
        return s->generate(count, sink);
      });
}
}  // namespace streams
}  // namespace video
}  // namespace satori