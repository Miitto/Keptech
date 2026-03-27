#pragma once

#include "keptech/rendering/texture.hpp"
#include <concepts>

namespace kt {
  template <typename T>
  concept CanRenderToFormat = requires(T a, TextureFormat format) {
    { a.canRenderToFormat(format) } -> std::same_as<bool>;
  };
} // namespace kt
