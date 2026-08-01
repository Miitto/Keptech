#pragma once

#include "keptech/core/bitflag.hpp"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

namespace kt {
  enum class WindowCreateFlagBits : uint32_t {
    Fullscreen = SDL_WINDOW_FULLSCREEN,
    Hidden = SDL_WINDOW_HIDDEN,
    Borderless = SDL_WINDOW_BORDERLESS,
    Resizable = SDL_WINDOW_RESIZABLE,
    Minimized = SDL_WINDOW_MINIMIZED,
    Maximized = SDL_WINDOW_MAXIMIZED,
    HighPixelDensity = SDL_WINDOW_HIGH_PIXEL_DENSITY,
    InputFocus = SDL_WINDOW_INPUT_FOCUS,
    MouseFocus = SDL_WINDOW_MOUSE_FOCUS,
    MouseGrabbed = SDL_WINDOW_MOUSE_GRABBED,
    MouseCapture = SDL_WINDOW_MOUSE_CAPTURE,
    AlwaysOnTop = SDL_WINDOW_ALWAYS_ON_TOP,
    Modal = SDL_WINDOW_MODAL,
    Utility = SDL_WINDOW_UTILITY,
    Tooltip = SDL_WINDOW_TOOLTIP,
    KeyboardGrabbed = SDL_WINDOW_KEYBOARD_GRABBED,
    Transparent = SDL_WINDOW_TRANSPARENT,
    NotFocusable = SDL_WINDOW_NOT_FOCUSABLE,
  };

  using WindowCreateFlags = Bitflag<WindowCreateFlagBits>;

  struct WindowCreateInfo {
    /// Title of the window.
    const char* title = "Keptech App";
    /// Width of the window in pixels.
    int width = -1;
    /// Height of the window in pixels.
    int height = -1;
    /// Flags for creating the window.
    WindowCreateFlags flags = WindowCreateFlags::none();
  };

  using WindowEvent = SDL_Event;

  class Window {
  public:
    Window(const WindowCreateInfo& createInfo);
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&& other) noexcept : handle(other.handle), shouldExit(other.shouldExit), size(other.size), renderSize(other.renderSize) {
      other.handle = nullptr;
    }
    Window& operator=(Window&& other) noexcept {
      if (this != &other) {
        handle = other.handle;
        shouldExit = other.shouldExit;
        size = other.size;
        renderSize = other.renderSize;

        other.handle = nullptr;
      }
      return *this;
    }

    ~Window() {
      if (handle) {
        SDL_DestroyWindow(handle);
      }
    }

    [[nodiscard]] bool shouldClose() const;

    bool pollEvent(WindowEvent& event);

    void requestClose() { shouldExit = true; }

    [[nodiscard]] glm::ivec2 getSize() const { return size; }
    [[nodiscard]] glm::ivec2 getRenderSize() const { return renderSize; }

    [[nodiscard]] SDL_Window* getHandle() const { return handle; }

    void grabMouse(bool grab);
    void hideCursor(bool hide);

    [[nodiscard]] bool isMouseGrabbed() const { return mouseGrabbed; }
    [[nodiscard]] bool isCursorHidden() const { return cursorHidden; }

    static bool init();
    static void shutdown();

  private:
    void updateSize();
    void updateRenderSize();

    SDL_Window* handle;
    bool shouldExit = false;
    bool mouseGrabbed = false;
    bool cursorHidden = false;

    glm::ivec2 size = {};
    glm::ivec2 renderSize = {};
  };
} // namespace kt
