#pragma once

#include <type_traits>
namespace kt {
  template <typename S, typename A>
    requires std::is_integral_v<S> && std::is_integral_v<A>
  S roundToAlignment(S size, A alignment) {
    S a = static_cast<S>(alignment);
    return (size + a - 1) & ~(a - 1);
  }

  uint32_t calcMipLevels(glm::uvec2 size);
  uint32_t calcMipLevels(glm::uvec3 size);
} // namespace kt
