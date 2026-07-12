#pragma once

#include <Volk/volk.h>
#include <concepts>

namespace kt {
  template <typename T>
  concept CanRenderToFormat = requires(T a, VkFormat format) {
    { a.canRenderToFormat(format) } -> std::same_as<bool>;
  };
} // namespace kt
