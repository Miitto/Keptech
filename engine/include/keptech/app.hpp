#pragma once

#include <concepts>
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

    virtual void update();

    [[nodiscard]] inline core::window::Window& getWindow() { return window; }
    [[nodiscard]] inline const core::window::Window& getWindow() const {
      return window;
    }

  private:
    core::window::Window window;
  };

  template <typename T>
    requires(std::derived_from<T, App> && !std::is_abstract_v<T> &&
             !std::is_same_v<T, App>)
  void run(App& app) {
    keptech::core::window::Window& window = app.getWindow();

    while (true) {
      {
        keptech::core::window::Window::Event event;
        while (window.pollEvent(event)) {
        }
      }
      if (window.shouldClose()) {
        break;
      }

      app.update();
    }
  }
} // namespace keptech
