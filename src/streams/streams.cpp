#include "streams.h"

#include <fstream>

namespace satori {
namespace video {

namespace streams {
publisher<std::string> read_lines(const std::string &filename) {
  struct state {
    std::ifstream input;

    void generate_one(streams::observer<std::string> &observer) {
      std::string line;
      if (!std::getline(input, line)) {
        observer.on_complete();
        return;
      }
      observer.on_next(std::move(line));
    }
  };

  return streams::generators<std::string>::stateful(
      [filename]() { return new state{std::ifstream(filename)}; },
      [](state *s, streams::observer<std::string> &sink) { s->generate_one(sink); });
}
}  // namespace streams
}  // namespace video
}  // namespace satori