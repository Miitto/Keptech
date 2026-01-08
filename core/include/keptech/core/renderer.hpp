#pragma once

#include <concepts>
#include <expected>
#include <string>

namespace keptech::core {
  class Scene;

  namespace window {
    class Window;
  }
} // namespace keptech::core

namespace keptech::core::renderer {

  struct CreateInfo {
    const char* applicationName = "Keptech App";
  };

  template <typename T>
  concept CRenderer =
      requires(T a, const CreateInfo& ci, const core::window::Window& w,
               core::Scene& scene) {
        { T::create(ci, w) } -> std::same_as<std::expected<T, std::string>>;
        { a.newFrame() } -> std::same_as<void>;
        { a.submitScene(scene) } -> std::same_as<void>;
        { a.render() } -> std::same_as<void>;
        { T::getName() } -> std::same_as<const char*>;
      };
} // namespace keptech::core::renderer
