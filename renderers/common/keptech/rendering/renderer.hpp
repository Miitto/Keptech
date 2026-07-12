#pragma once

#include "keptech/core/scene.hpp"
#include <concepts>
#include <expected>
#include <keptech/core/window.hpp>
#include <string>

namespace kt {
  struct RendererCreateInfo {
    const char* applicationName = "Keptech App";
  };

  template <typename T>
  concept CRenderer = requires(T a, const RendererCreateInfo& ci, const core::window::Window& w, Scene& scene) {
    { T::create(ci, w) } -> std::same_as<std::expected<T, std::string>>;
    { a.newFrame() } -> std::same_as<void>;
    { a.render() } -> std::same_as<void>;
  };

} // namespace kt
