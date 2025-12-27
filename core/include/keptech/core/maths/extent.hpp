#pragma once

#include <glm/glm.hpp>

namespace keptech::core::maths {
  template <typename T> struct Extent2D {
    glm::vec<2, T> offset{};
    glm::vec<2, T> size{};

    glm::vec<2, T> max() const { return offset + size; }
  };

  template <typename T> struct Extent3D {
    glm::vec<3, T> offset{};
    glm::vec<3, T> size{};

    glm::vec<3, T> max() const { return offset + size; }
  };

  using Extent2Df = Extent2D<float>;
  using Extent2Di = Extent2D<int>;
  using Extent2Du = Extent2D<unsigned int>;
  using Extent3Df = Extent3D<float>;
  using Extent3Di = Extent3D<int>;
  using Extent3Du = Extent3D<unsigned int>;
} // namespace keptech::core::maths
