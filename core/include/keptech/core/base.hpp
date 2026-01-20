#pragma once

#include <glm/glm.hpp>

namespace keptech::core {
  /// Timestep in milliseconds
  using Timestep = float;

  constexpr glm::vec3 FORWARD{0, 0, 1};
  constexpr glm::vec3 RIGHT{1, 0, 0};
  constexpr glm::vec3 UP{0, 1, 0};
  constexpr glm::vec3 ORIGIN{0, 0, 0};

  template <typename... T> struct overloaded : T... {
    using T::operator()...;
  };
} // namespace keptech::core
