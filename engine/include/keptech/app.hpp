#pragma once

#include <concepts>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/imgui.h>
#include <keptech/core/cameras/cameraManager.hpp>
#include <keptech/core/renderer.hpp>
#include <keptech/core/window.hpp>
#include <keptech/ecs/ecs.hpp>

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

  protected:
    core::window::Window window;
  };

  template <typename T>
  concept AppDerived = std::derived_from<T, App> && !std::is_abstract_v<T>;

  template <typename OnEvent>
  concept EventHandler = std::invocable<OnEvent, core::window::Window&,
                                        const core::window::Window::Event&>;

  template <typename T, class R>
    requires(AppDerived<T> && core::renderer::CRenderer<R>)
  void run(T& app, R& renderer) {
    auto& ecs = keptech::ecs::ECS::get();

    ecs.registerSystem<core::cameras::CameraManager>(
        core::cameras::CameraManager::getSignature());

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

    keptech::ecs::FrameData frameData{};

    auto now = std::chrono::high_resolution_clock::now();

    while (true) {
      {
        keptech::core::window::Window::Event event;
        while (window.pollEvent(event)) {
          ImGui_ImplSDL3_ProcessEvent(&event);
          if ((io.WantCaptureKeyboard && isKeyboardEvent(event)) ||
              (io.WantCaptureMouse && isMouseEvent(event))) {
            continue;
          }
          app.onEvent(event);
        }
      }
      if (window.shouldClose()) {
        break;
      }

      auto newTime = std::chrono::high_resolution_clock::now();

      float dt = std::chrono::duration<float, std::chrono::seconds::period>(
                     newTime - now)
                     .count();

      frameData.dt = dt;

      now = newTime;

      renderer.newFrame();

      ecs.preUpdateAllSystems(frameData);
      ecs.updateAllSystems(frameData);
      ecs.postUpdateAllSystems(frameData);

      renderer.render();
    }

    ecs.destroy();
  }
} // namespace keptech
