#include <boost/program_options.hpp>
#include <gsl/gsl>
#include <memory>
#include <thread>
#include "SDL2/SDL.h"

#include "cli_streams.h"
#include "data.h"
#include "logging_impl.h"
#include "streams/streams.h"
#include "streams/threaded_worker.h"
#include "threadutils.h"
#include "video_streams.h"

using namespace satori::video;

namespace {

struct rtm_error_handler : rtm::error_callbacks {
  void on_error(std::error_condition ec) override { LOG(ERROR) << ec.message(); }
};

namespace po = boost::program_options;

cli_streams::cli_options cli_configuration() {
  cli_streams::cli_options result;
  result.enable_rtm_input = true;
  result.enable_file_input = true;
  result.enable_camera_input = true;
  result.enable_generic_input_options = true;
  result.enable_url_input = true;

  return result;
}

po::options_description cli_options() {
  po::options_description cli_generic("Generic options");
  cli_generic.add_options()("help", "produce help message");
  cli_generic.add_options()(
      ",v", po::value<std::string>(),
      "log verbosity level (INFO, WARNING, ERROR, FATAL, OFF, 1-9)");

  return cli_generic;
}

struct player_configuration : cli_streams::configuration {
  player_configuration(int argc, char *argv[])
      : configuration(argc, argv, cli_configuration(), cli_options()) {}
};

class sdl_window {
 public:
  sdl_window() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
      ABORT() << "SDL could not initialize! SDL Error: " << SDL_GetError();
    }

    if (SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1") != SDL_TRUE) {
      LOG(WARNING) << "Linear texture filtering not enabled!";
    }

    _ptr = SDL_CreateWindow("Satori Video Player", SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_RESIZABLE);

    CHECK_NOTNULL(_ptr) << "Window could not be created! SDL Error: " << SDL_GetError();
  }

  sdl_window(const sdl_window &) = delete;
  sdl_window(sdl_window &&) = delete;

  ~sdl_window() {
    LOG(INFO) << "Destroying window";
    SDL_DestroyWindow(_ptr);
  }

 private:
  friend class sdl_renderer;

  SDL_Window *_ptr{nullptr};
};

class sdl_surface {
 public:
  explicit sdl_surface(const owned_image_frame &frame) {
    _ptr = SDL_CreateRGBSurfaceFrom((void *)frame.plane_data[0].data(), frame.width,
                                    frame.height, 24, frame.width * 3, 0x00ff0000,
                                    0x0000ff00, 0x000000ff, 0x00000000);

    CHECK_NOTNULL(_ptr) << "Unable to create surface! SDL Error: " << SDL_GetError();
  }

  sdl_surface(const sdl_surface &) = delete;

  sdl_surface(sdl_surface &&s) noexcept { std::swap(_ptr, s._ptr); }

  ~sdl_surface() {
    LOG(5) << "Destroying surface";
    if (_ptr != nullptr) {
      SDL_FreeSurface(_ptr);
    }
  }

 private:
  friend class sdl_renderer;

  SDL_Surface *_ptr{nullptr};
};

class sdl_renderer {
 public:
  explicit sdl_renderer(sdl_window &w) {
    _ptr = SDL_CreateRenderer(w._ptr, -1, SDL_RENDERER_ACCELERATED);
    CHECK_NOTNULL(_ptr) << "Renderer could not be created! SDL Error: " << SDL_GetError();

    SDL_SetRenderDrawColor(_ptr, 0xff, 0xff, 0xff, 0xff);
  }

  sdl_renderer(const sdl_renderer &) = delete;
  sdl_renderer(sdl_renderer &&) = delete;

  ~sdl_renderer() {
    LOG(INFO) << "Destroying renderer";
    SDL_DestroyRenderer(_ptr);
  }

  void render(sdl_surface &&surface) {
    SDL_Texture *texture = SDL_CreateTextureFromSurface(_ptr, surface._ptr);
    CHECK(texture) << "Unable to create texture! SDL Error: " << SDL_GetError();

    auto texture_destroyer = gsl::finally([texture]() {
      LOG(5) << "Destroying texture";
      SDL_DestroyTexture(texture);
    });

    SDL_RenderClear(_ptr);
    SDL_RenderCopy(_ptr, texture, nullptr, nullptr);
    SDL_RenderPresent(_ptr);
  }

 private:
  SDL_Renderer *_ptr{nullptr};
};

streams::op<owned_image_packet, sdl_surface> image_to_surface() {
  struct packet_visitor : public boost::static_visitor<streams::publisher<sdl_surface>> {
    streams::publisher<sdl_surface> operator()(
        const owned_image_metadata & /*metadata*/) {
      return streams::publishers::empty<sdl_surface>();
    }

    streams::publisher<sdl_surface> operator()(const owned_image_frame &frame) {
      std::vector<sdl_surface> result;
      result.emplace_back(frame);
      return streams::publishers::of(std::move(result));
    }
  };

  struct packet_visitor packet_visitor;

  return [&packet_visitor](streams::publisher<owned_image_packet> &&src) {
    return std::move(src) >> streams::flat_map([&packet_visitor](owned_image_packet &&p) {
             return boost::apply_visitor(packet_visitor, p);
           });
  };
}

void run_sdl_loop(sdl_renderer &renderer, std::queue<sdl_surface> &surfaces,
                  std::mutex &surfaces_mutex) {
  while (true) {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_QUIT) {
        return;
      }

      SDL_Delay(5);
    }

    std::lock_guard<std::mutex> lock(surfaces_mutex);
    if (!surfaces.empty()) {
      renderer.render(std::move(surfaces.front()));
      surfaces.pop();
    }
  }
}
}  // namespace

int main(int argc, char *argv[]) {
  player_configuration config{argc, argv};

  init_logging(argc, argv);

  sdl_window window;
  sdl_renderer renderer{window};

  boost::asio::io_service io_service;
  boost::asio::ssl::context ssl_context{boost::asio::ssl::context::sslv23};
  struct rtm_error_handler rtm_error_handler;

  // TODO (needs fix): pass asio thread id instead of current
  std::shared_ptr<rtm::client> rtm_client = config.rtm_client(
      io_service, std::this_thread::get_id(), ssl_context, rtm_error_handler);
  if (rtm_client) {
    auto ec = rtm_client->start();
    if (ec) {
      ABORT() << "error starting rtm client: " << ec.message();
    }
  }

  std::string rtm_channel = config.rtm_channel();

  streams::publisher<sdl_surface> source =
      config.decoded_publisher(io_service, rtm_client, image_pixel_format::BGR)
      >> streams::threaded_worker("player.image_buffer") >> streams::flatten()
      >> image_to_surface() >> streams::threaded_worker("player.surface_buffer")
      >> streams::flatten() >> streams::do_finally([&io_service, &rtm_client]() {
          io_service.post([&rtm_client]() {
            if (rtm_client) {
              if (auto ec = rtm_client->stop()) {
                LOG(ERROR) << "error stopping rtm client: " << ec.message();
              } else {
                LOG(INFO) << "rtm client was stopped";
              }
            }
          });
        });

  std::queue<sdl_surface> surfaces;
  std::mutex surfaces_mutex;

  auto when_done = source->process([&surfaces, &surfaces_mutex](sdl_surface &&surface) {
    std::lock_guard<std::mutex> lock(surfaces_mutex);
    surfaces.push(std::move(surface));
  });
  when_done.on([](std::error_condition ec) {
    if (ec) {
      LOG(ERROR) << "Error while playing: " << ec.message();
    }
  });
  std::thread([&io_service]() {
    threadutils::set_current_thread_name("asio-loop");
    io_service.run();
  })
      .detach();
  run_sdl_loop(renderer, surfaces, surfaces_mutex);
  SDL_Quit();

  io_service.stop();

  // TODO: this one fails if window is closed before file is fully played
  // CHECK(when_done.resolved());

  LOG(INFO) << "Done";
}
