#include <SDL3/SDL_video.h>
#include <keptech/core/window.hpp>

#include "keptech/core/kt-logger.hpp"

namespace kt {
  bool Window::init() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
      auto msg = SDL_GetError();
      KT_ERROR("Failed to init SDL: {}", msg);
      return false;
    }
    return true;
  }
  void Window::shutdown() { SDL_Quit(); }

  Window::Window(const WindowCreateInfo& info)
      : handle(SDL_CreateWindow(info.title, info.width, info.height,
#ifdef KT_VULKAN
                                SDL_WINDOW_VULKAN |
#endif
                                    static_cast<SDL_WindowFlags>(info.flags))) {
    updateSize();
    updateRenderSize();

    KT_DEBUG("Window created with size {}x{} and render size {}x{} (Requested {}x{})", size.x, size.y, renderSize.x, renderSize.y,
             info.width, info.height);
  }

  bool Window::shouldClose() const { return shouldExit; }

  void Window::grabMouse(bool grab) {
    auto err = SDL_SetWindowMouseGrab(handle, grab);
    KT_ASSERT(err, "Failed to set mouse grab: {}", SDL_GetError());
    err = SDL_SetWindowRelativeMouseMode(handle, grab);
    KT_ASSERT(err, "Failed to set relative mouse mode: {}", SDL_GetError());
    mouseGrabbed = grab;
    KT_TRACE("Mouse {}", grab ? "grabbed" : "released");
  }
  void Window::hideCursor(bool hide) {
    auto err = hide ? SDL_HideCursor() : SDL_ShowCursor();
    KT_ASSERT(err >= 0, "Failed to set cursor visibility: {}", SDL_GetError());
    cursorHidden = hide;
    KT_TRACE("Cursor {}", hide ? "hidden" : "visible");
  }

  bool Window::pollEvent(WindowEvent& event) {
    if (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_EVENT_QUIT: {
        KT_INFO("Exit requested");
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

} // namespace kt
