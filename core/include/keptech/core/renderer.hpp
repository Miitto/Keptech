#pragma once

#include "keptech/core/window.hpp"
#include "keptech/ecs/system.hpp"
#include <concepts>

namespace keptech::core::renderer {

  struct CreateInfo {
    const char* applicationName = "Keptech App";
  };

  class Renderer : public ecs::System {};

  template <typename T>
  concept CRenderer =
      requires(T a, const CreateInfo& ci, const core::window::Window& w) {
        { T::create(ci, w) } -> std::same_as<std::expected<T*, std::string>>;
        { a.newFrame() } -> std::same_as<void>;
        { a.render() } -> std::same_as<void>;
        { T::getName() } -> std::same_as<const char*>;
      } && std::derived_from<T, Renderer>;
} // namespace keptech::core::renderer
