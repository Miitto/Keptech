#pragma once

#include <concepts>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/imgui.h>
#include <keptech/core/renderer.hpp>
#include <keptech/core/window.hpp>

namespace keptech {
  class App {
  public:
    App() = delete;
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;
    virtual ~App() = default;

    App(core::window::Window&& window) : window(std::move(window)) {}

    virtual void update() = 0;
    virtual void onEvent(const core::window::Window::Event&) = 0;

    [[nodiscard]] inline core::window::Window& getWindow() { return window; }
    [[nodiscard]] inline const core::window::Window& getWindow() const {
      return window;
    }

  private:
    core::window::Window window;
  };

  template <typename T>
  concept AppDerived = std::derived_from<T, App> && !std::is_abstract_v<T>;

  template <typename OnEvent>
  concept EventHandler = std::invocable<OnEvent, core::window::Window&,
                                        const core::window::Window::Event&>;

  template <typename T, class R>
    requires(AppDerived<T> && core::renderer::Renderer<R>)
  void run(T& app, R& renderer) {
    keptech::core::window::Window& window = app.getWindow();

    auto& io = ImGui::GetIO();

    auto isKeyboardEvent = [](core::window::Window::Event event) {
      switch (event.type) {
      case SDL_EVENT_KEY_DOWN:
      case SDL_EVENT_KEY_UP:
      case SDL_EVENT_TEXT_INPUT:
        return true;
      default:
        return false;
      }
    };

    auto isMouseEvent = [](core::window::Window::Event event) {
      switch (event.type) {
      case SDL_EVENT_MOUSE_MOTION:
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
      case SDL_EVENT_MOUSE_BUTTON_UP:
      case SDL_EVENT_MOUSE_WHEEL:
      case SDL_EVENT_FINGER_DOWN:
      case SDL_EVENT_FINGER_UP:
      case SDL_EVENT_FINGER_MOTION:
        return true;
      default:
        return false;
      }
    };

    while (true) {
      {
        keptech::core::window::Window::Event event;
        while (window.pollEvent(event)) {
          ImGui_ImplSDL3_ProcessEvent(&event);
          if ((io.WantCaptureKeyboard && isKeyboardEvent(event)) ||
              io.WantCaptureMouse && isMouseEvent(event)) {
            continue;
          }
          app.onEvent(event);
        }
      }
      if (window.shouldClose()) {
        break;
      }

      renderer.newFrame();

      app.update();

      renderer.render();
    }
  }
} // namespace keptech
