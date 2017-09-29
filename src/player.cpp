#include <boost/program_options.hpp>
#include <memory>
#include <thread>
#include "SDL2/SDL.h"

#include "logging_impl.h"

#include "avutils.h"
#include "cli_streams.h"
#include "error.h"
#include "librtmvideo/data.h"
#include "streams.h"
#include "video_streams.h"
#include "worker.h"

namespace {

constexpr size_t incoming_encoded_frames_max_buffer_size = 1024;
constexpr size_t image_frames_max_buffer_size = 1024;
constexpr size_t surfaces_max_buffer_size = 1024;
constexpr size_t textures_max_buffer_size = 1024;

struct rtm_error_handler : public rtm::error_callbacks {
  void on_error(std::error_condition ec) override { LOG(ERROR) << ec.message(); }
};

namespace po = boost::program_options;

rtm::video::cli_streams::configuration cli_configuration() {
  rtm::video::cli_streams::configuration result;
  result.enable_rtm_input = true;
  result.enable_file_input = true;
  result.enable_camera_input = true;

  return result;
}

po::options_description cli_options(
    const rtm::video::cli_streams::configuration &cli_cfg) {
  po::options_description cli_dimensions("Video stream downscaling options");
  cli_dimensions.add_options()("stream-dimensions",
                               po::value<std::string>()->default_value("original"),
                               "'original' or '<width>x<height>', for example, 320x240");

  po::options_description cli_generic("Generic options");
  cli_generic.add_options()("help", "produce help message");
  cli_generic.add_options()(
      ",v", po::value<std::string>(),
      "log verbosity level (INFO, WARNING, ERROR, FATAL, OFF, 1-9)");

  po::options_description result = cli_cfg.to_boost();
  result.add(cli_dimensions);
  result.add(cli_generic);

  return result;
}

po::variables_map cli_parse(int argc, char *argv[],
                            const rtm::video::cli_streams::configuration &cli_cfg,
                            const po::options_description &cli_options) {
  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, cli_options), vm);
    po::notify(vm);
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    std::cerr << cli_options << std::endl;
    exit(1);
  }

  if (argc == 1 || vm.count("help")) {
    std::cerr << cli_options << std::endl;
    exit(1);
  }

  if (!cli_cfg.validate(vm)) exit(1);

  std::string dimensions_str = vm["stream-dimensions"].as<std::string>();
  if (dimensions_str != "original") {
    boost::optional<rtm::video::image_size> dimensions =
        rtm::video::avutils::parse_image_size(dimensions_str);

    if (!dimensions) {
      std::cerr << "invalid value provided for dimensions\n";
      exit(1);
    }
  }

  return vm;
}

void stream_image_size_from_cli(const po::variables_map &vm, int16_t &width,
                                int16_t &height) {
  std::string dimensions_str = vm["stream-dimensions"].as<std::string>();
  if (dimensions_str == "original") {
    width = ORIGINAL_IMAGE_WIDTH;
    height = ORIGINAL_IMAGE_HEIGHT;
    return;
  }

  boost::optional<rtm::video::image_size> dimensions =
      rtm::video::avutils::parse_image_size(dimensions_str);

  width = dimensions->width;
  height = dimensions->height;
}

std::shared_ptr<SDL_Window> create_window() {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    LOG(ERROR) << "SDL could not initialize! SDL Error: " << SDL_GetError();
    return nullptr;
  }

  if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"))
    LOG(WARNING) << "Linear texture filtering not enabled!";

  std::shared_ptr<SDL_Window> window(
      SDL_CreateWindow("SDL Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                       640, 480, SDL_WINDOW_RESIZABLE),
      [](SDL_Window *ptr) {
        LOG(INFO) << "Destroying window";
        SDL_DestroyWindow(ptr);
      });

  if (!window) LOG(ERROR) << "Window could not be created! SDL Error: " << SDL_GetError();

  return window;
}

std::shared_ptr<SDL_Renderer> create_renderer(std::shared_ptr<SDL_Window> window) {
  std::shared_ptr<SDL_Renderer> renderer(
      SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED),
      [](SDL_Renderer *ptr) {
        LOG(INFO) << "Destroying renderer";
        SDL_DestroyRenderer(ptr);
      });

  if (!renderer) {
    LOG(ERROR) << "Renderer could not be created! SDL Error: " << SDL_GetError();
    return nullptr;
  }

  SDL_SetRenderDrawColor(renderer.get(), 0xff, 0xff, 0xff, 0xff);

  return renderer;
}

streams::op<rtm::video::owned_image_packet, std::shared_ptr<SDL_Surface>>
image_to_surface() {
  return [](streams::publisher<rtm::video::owned_image_packet> &&src) {

    streams::publisher<std::shared_ptr<SDL_Surface>> result =
        std::move(src) >> streams::flat_map([](rtm::video::owned_image_packet &&p) {
          if (const auto *f = boost::get<rtm::video::owned_image_frame>(&p)) {
            std::shared_ptr<SDL_Surface> surface(
                SDL_CreateRGBSurfaceFrom((void *)f->plane_data[0].data(), f->width,
                                         f->height, 24, f->width * 3, 0x00ff0000,
                                         0x0000ff00, 0x000000ff, 0x00000000),
                [](SDL_Surface *ptr) {
                  LOG(INFO) << "Destroying surface";
                  SDL_FreeSurface(ptr);
                });

            if (!surface) {
              LOG(ERROR) << "Unable to create surface! SDL Error: " << SDL_GetError();
              return streams::publishers::error<std::shared_ptr<SDL_Surface>>(
                  rtm::video::video_error::FrameGenerationError);
            }

            return streams::publishers::of({surface});
          } else if (const auto *m = boost::get<rtm::video::owned_image_metadata>(&p)) {
            return streams::publishers::empty<std::shared_ptr<SDL_Surface>>();
          } else {
            CHECK(false) << "unsupported variant";
            return streams::publishers::empty<std::shared_ptr<SDL_Surface>>();
          }
        });

    return result;
  };
}

streams::op<std::shared_ptr<SDL_Surface>, std::shared_ptr<SDL_Texture>>
surface_to_texture(std::shared_ptr<SDL_Renderer> renderer) {
  return [renderer](streams::publisher<std::shared_ptr<SDL_Surface>> &&src) {

    streams::publisher<std::shared_ptr<SDL_Texture>> result =
        std::move(src)
        >> streams::flat_map([renderer](std::shared_ptr<SDL_Surface> &&surface) {
            std::shared_ptr<SDL_Texture> texture(
                SDL_CreateTextureFromSurface(renderer.get(), surface.get()),
                [](SDL_Texture *ptr) {
                  LOG(INFO) << "Destroying texture";
                  SDL_DestroyTexture(ptr);
                });

            if (!texture) {
              LOG(ERROR) << "Unable to create texture! SDL Error: " << SDL_GetError();
              return streams::publishers::error<std::shared_ptr<SDL_Texture>>(
                  rtm::video::video_error::FrameGenerationError);
            }

            return streams::publishers::of({texture});
          });

    return result;
  };
}

void run_sdl_loop() {
  bool running{true};
  SDL_Event e;
  while (running) {
    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_QUIT) {
        running = false;
      } else {
        SDL_Delay(5);
      }
    }
  }
}
}  // namespace

int main(int argc, char *argv[]) {
  rtm::video::cli_streams::configuration cli_cfg = cli_configuration();
  po::variables_map vm = cli_parse(argc, argv, cli_cfg, cli_options(cli_cfg));

  init_logging(argc, argv);

  int16_t stream_width{ORIGINAL_IMAGE_WIDTH};
  int16_t stream_height{ORIGINAL_IMAGE_HEIGHT};
  stream_image_size_from_cli(vm, stream_width, stream_height);

  std::shared_ptr<SDL_Window> window = create_window();
  if (!window) {
    LOG(ERROR) << "Failed to create window";
    return -1;
  }

  std::shared_ptr<SDL_Renderer> renderer = create_renderer(window);
  if (!renderer) {
    LOG(ERROR) << "Failed to create renderer";
    return -1;
  }

  boost::asio::io_service io_service;
  boost::asio::ssl::context ssl_context{boost::asio::ssl::context::sslv23};
  struct rtm_error_handler rtm_error_handler;

  std::shared_ptr<rtm::client> rtm_client =
      cli_cfg.rtm_client(vm, io_service, ssl_context, rtm_error_handler);
  if (rtm_client) rtm_client->start();

  std::string rtm_channel = cli_cfg.rtm_channel(vm);

  streams::publisher<std::shared_ptr<SDL_Texture>> source =
      cli_cfg.encoded_publisher(vm, io_service, rtm_client, rtm_channel, true)
      >> rtm::video::buffered_worker("player.encoded_buffer",
                                     incoming_encoded_frames_max_buffer_size)
      >> rtm::video::decode_image_frames(stream_width, stream_height,
                                         image_pixel_format::BGR)
      >> rtm::video::buffered_worker("player.image_buffer", image_frames_max_buffer_size)
      >> image_to_surface()
      >> rtm::video::buffered_worker("player.surface_buffer", surfaces_max_buffer_size)
      >> surface_to_texture(renderer)
      >> rtm::video::buffered_worker("player.texture_buffer", textures_max_buffer_size)
      >> streams::do_finally([&rtm_client]() {
          if (rtm_client) rtm_client->stop();
        });

  source->process(
      [renderer](std::shared_ptr<SDL_Texture> &&texture) {
        SDL_RenderClear(renderer.get());
        SDL_RenderCopy(renderer.get(), texture.get(), nullptr, nullptr);
        SDL_RenderPresent(renderer.get());
      },
      []() { LOG(INFO) << "got complete"; },
      [](std::error_condition ec) { LOG(ERROR) << "got error: " << ec.message(); });

  std::thread([&io_service]() { io_service.run(); }).detach();

  run_sdl_loop();

  SDL_Quit();

  io_service.stop();

  LOG(INFO) << "Done";
}
