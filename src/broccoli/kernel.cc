#include "broccoli/kernel.hh"

#include "SDL.h"

namespace broccoli {
  Kernel::Kernel(const char *caption, int width, int height)
  : m_sdl_window(nullptr),
    m_is_running(false)
  {
    int sdl_init_error = SDL_Init(SDL_INIT_VIDEO);
    CHECK(
      sdl_init_error == 0,
      [] () { return fmt::format("Failed to initialize SDL2: {}", SDL_GetError()); }
    );

    m_sdl_window = SDL_CreateWindow(
      caption, 
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
      width, height, 
      SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI
    );
    CHECK(
      m_sdl_window != nullptr, 
      [] () { return fmt::format("Failed to create an SDL2 window: {}", SDL_GetError()); }
    );
  }
  Kernel::~Kernel() {
    SDL_DestroyWindow(reinterpret_cast<SDL_Window*>(m_sdl_window));
    SDL_Quit();
  }
}
namespace broccoli {
  void Kernel::run() {
    m_is_running = true;
    SDL_ShowWindow(reinterpret_cast<SDL_Window*>(m_sdl_window));
    while (m_is_running) {
      dispatch_events();
      update();
      draw();
    }
  }
}
namespace broccoli {
  void Kernel::halt() {
    m_is_running = false;
  }
}
namespace broccoli {
  void Kernel::dispatch_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        // TODO: expand event-handling.
        case SDL_QUIT:
          m_is_running = false;
          break;
        default:
          break;
      }
    }
  }
  void Kernel::draw() {
  }
  void Kernel::update() {
  }
}