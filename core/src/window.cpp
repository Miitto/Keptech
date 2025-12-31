#include <SDL3/SDL_oldnames.h>
#include <keptech/core/window.hpp>

namespace keptech::core::window {
  void init() { SDL_Init(SDL_INIT_VIDEO); }
  void shutdown() { SDL_Quit(); }

  Window::Window(const CreateInfo& info)
      : handle(
            SDL_CreateWindow(info.title, info.width, info.height, SDL_WINDOW_VULKAN | info.flags)) {
    updateSize();
    updateRenderSize();
  }

  bool Window::shouldClose() const { return shouldExit; }

  bool Window::pollEvent(Event& event) {
    if (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_EVENT_QUIT: {
        shouldExit = true;
        break;
      }
      case SDL_EVENT_WINDOW_RESIZED: {
        updateSize();
        updateRenderSize();
        break;
      }
      case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
        updateRenderSize();
        break;
      }
      default:;
      }
      return true;
    }
    return false;
  }

  void Window::updateSize() {
    int w = 0, h = 0;
    SDL_GetWindowSize(handle, &w, &h);
    size = glm::ivec2(w, h);
  }

  void Window::updateRenderSize() {
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(handle, &w, &h);
    renderSize = glm::ivec2(w, h);
  }

} // namespace keptech::core::window
