#include <boost/assert.hpp>
#include <boost/program_options.hpp>
#include <memory>
#include <thread>
#include "SDL2/SDL.h"

#include "avutils.h"
#include "cli_streams.h"
#include "librtmvideo/data.h"
#include "logging_implementation.h"
#include "producer_consumer_queue.h"
#include "streams.h"
#include "video_streams.h"
#include "worker.h"

namespace {

namespace po = boost::program_options;

constexpr size_t incoming_encoded_frames_max_buffer_size = 1024;
constexpr size_t image_frames_max_buffer_size = 1024;

struct rtm_error_handler : public rtm::error_callbacks {
  void on_error(std::error_condition ec) override { LOG_S(ERROR) << ec.message(); }
};

using surfaces_queue = producer_consumer_queue<std::shared_ptr<SDL_Surface>>;

struct video_context {
  std::shared_ptr<SDL_Window> window;
  std::shared_ptr<SDL_Renderer> renderer;
};

bool init_video_context(video_context &ctx) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    LOG_S(ERROR) << "SDL could not initialize! SDL Error: " << SDL_GetError();
    return false;
  }

  if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"))
    LOG_S(WARNING) << "Linear texture filtering not enabled!";

  std::shared_ptr<SDL_Window> window(
      SDL_CreateWindow("SDL Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                       640, 480, SDL_WINDOW_RESIZABLE),
      [](SDL_Window *ptr) {
        LOG_S(INFO) << "Destroying window";
        SDL_DestroyWindow(ptr);
      });
  if (!window) {
    LOG_S(ERROR) << "Window could not be created! SDL Error: " << SDL_GetError();
    return false;
  }

  std::shared_ptr<SDL_Renderer> renderer(
      SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED),
      [](SDL_Renderer *ptr) {
        LOG_S(INFO) << "Destroying renderer";
        SDL_DestroyRenderer(ptr);
      });
  if (!renderer) {
    LOG_S(ERROR) << "Renderer could not be created! SDL Error: " << SDL_GetError();
    return false;
  }

  SDL_SetRenderDrawColor(renderer.get(), 0xff, 0xff, 0xff, 0xff);

  ctx.window = window;
  ctx.renderer = renderer;

  return true;
}

std::shared_ptr<SDL_Texture> surface_to_texture(
    const std::shared_ptr<SDL_Surface> surface, const video_context &ctx) {
  std::shared_ptr<SDL_Texture> texture(
      SDL_CreateTextureFromSurface(ctx.renderer.get(), surface.get()),
      [](SDL_Texture *ptr) {
        LOG_S(INFO) << "Destroying texture";
        SDL_DestroyTexture(ptr);
      });

  if (!texture) LOG_S(ERROR) << "Unable to create texture! SDL Error: " << SDL_GetError();

  return texture;
}

streams::publisher<rtm::video::owned_image_packet> create_image_source(
    const rtm::video::cli_streams::configuration &cli_cfg, const po::variables_map &vm,
    boost::asio::io_service &io_service, boost::asio::ssl::context &ssl_context,
    struct rtm_error_handler &rtm_error_handler, int16_t stream_width,
    int16_t stream_height) {
  std::shared_ptr<rtm::client> rtm_client =
      cli_cfg.rtm_client(vm, io_service, ssl_context, rtm_error_handler);

  if (rtm_client) rtm_client->start();

  std::string rtm_channel = cli_cfg.rtm_channel(vm);
  return cli_cfg.encoded_publisher(vm, io_service, rtm_client, rtm_channel, true)
         >> rtm::video::buffered_worker("recorder.encoded_buffer",
                                        incoming_encoded_frames_max_buffer_size)
         >> rtm::video::decode_image_frames(stream_width, stream_height,
                                            image_pixel_format::BGR)
         >> rtm::video::buffered_worker("recorder.image_buffer",
                                        image_frames_max_buffer_size)
         >> streams::do_finally([&rtm_client]() {
             if (rtm_client) rtm_client->stop();
           });
}

void set_callbacks_on_source(streams::publisher<rtm::video::owned_image_packet> &&source,
                             surfaces_queue &surfaces) {
  source->process(
      [&surfaces](rtm::video::owned_image_packet &&p) {
        if (const rtm::video::owned_image_frame *frame =
                boost::get<rtm::video::owned_image_frame>(&p)) {
          std::shared_ptr<SDL_Surface> surface(
              SDL_CreateRGBSurfaceFrom((void *)frame->plane_data[0].data(), frame->width,
                                       frame->height, 24, frame->width * 3, 0x00ff0000,
                                       0x0000ff00, 0x000000ff, 0x00000000),
              [](SDL_Surface *ptr) {
                LOG_S(INFO) << "Destroying surface";
                SDL_FreeSurface(ptr);
              });

          if (surface) surfaces.put(std::move(surface));
        }
      },
      []() { LOG_S(INFO) << "got complete"; },
      [](std::error_condition ec) { LOG_S(ERROR) << "got error: " << ec.message(); });
}

void run_sdl_loop(struct video_context &video_context, surfaces_queue &surfaces) {
  bool running{true};
  SDL_Event e;
  while (running) {
    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_QUIT) {
        running = false;
      }
    }

    std::shared_ptr<SDL_Surface> surface;
    if (surfaces.poll(surface)) {
      std::shared_ptr<SDL_Texture> texture = surface_to_texture(surface, video_context);
      SDL_RenderClear(video_context.renderer.get());
      SDL_RenderCopy(video_context.renderer.get(), texture.get(), nullptr, nullptr);
      SDL_RenderPresent(video_context.renderer.get());
    } else {
      SDL_Delay(5);
    }
  }
}
}  // namespace

int main(int argc, char *argv[]) {
  po::options_description cli_dimensions("Video stream downscaling options");
  cli_dimensions.add_options()("stream-dimensions",
                               po::value<std::string>()->default_value("original"),
                               "'original' or '<width>x<height>', for example, 320x240");

  po::options_description cli_generic("Generic options");
  cli_generic.add_options()("help", "produce help message");
  cli_generic.add_options()(
      ",v", po::value<std::string>(),
      "log verbosity level (INFO, WARNING, ERROR, FATAL, OFF, 1-9)");

  rtm::video::cli_streams::configuration cli_cfg;
  cli_cfg.enable_rtm_input = true;
  cli_cfg.enable_file_input = true;
  cli_cfg.enable_camera_input = true;

  po::options_description cli_options = cli_cfg.to_boost();
  cli_options.add(cli_dimensions);
  cli_options.add(cli_generic);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, cli_options), vm);
  po::notify(vm);

  if (argc == 1 || vm.count("help")) {
    std::cerr << cli_options << "\n";
    exit(1);
  }

  if (!cli_cfg.validate(vm)) return -1;

  int16_t stream_width{ORIGINAL_IMAGE_WIDTH};
  int16_t stream_height{ORIGINAL_IMAGE_HEIGHT};
  std::string dimensions_str = vm["stream-dimensions"].as<std::string>();
  if (dimensions_str != "original") {
    boost::optional<rtm::video::image_size> dimensions =
        rtm::video::avutils::parse_image_size(dimensions_str);

    if (!dimensions) {
      std::cerr << "invalid value provided for dimensions\n";
      exit(1);
    }
    stream_width = dimensions->width;
    stream_height = dimensions->height;
  }

  init_logging(argc, argv);

  struct video_context video_context;
  surfaces_queue surfaces;

  if (!init_video_context(video_context)) {
    LOG_S(ERROR) << "Failed to initialize video context";
    exit(1);
  }

  boost::asio::io_service io_service;
  boost::asio::ssl::context ssl_context{boost::asio::ssl::context::sslv23};
  struct rtm_error_handler rtm_error_handler;

  streams::publisher<rtm::video::owned_image_packet> source =
      create_image_source(cli_cfg, vm, io_service, ssl_context, rtm_error_handler,
                          stream_width, stream_height);
  set_callbacks_on_source(std::move(source), surfaces);

  std::thread([&io_service]() { io_service.run(); }).detach();

  run_sdl_loop(video_context, surfaces);

  SDL_Quit();

  io_service.stop();

  LOG_S(INFO) << "Done";
}
