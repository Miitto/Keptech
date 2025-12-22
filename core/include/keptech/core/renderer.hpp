#pragma once

#include "keptech/core/ecs/system.hpp"
#include <concepts>

namespace keptech::core::renderer {

  class Renderer : public ecs::System {};

  template <typename T>
  concept CRenderer = requires(T a) {
    { a.newFrame() } -> std::same_as<void>;
    { a.render() } -> std::same_as<void>;
  } && std::derived_from<T, Renderer>;
} // namespace keptech::core::renderer
