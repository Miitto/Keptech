#pragma once

#include <glm/glm.hpp>

namespace kt::maths {
  template <size_t D, typename O, typename S = O> struct Rect {
    O x;
    O y;
    S width;
    S height;

    [[nodiscard]] glm::vec<D, O> max() const {
      return glm::vec<D, O>{x, y} +
             static_cast<O>(glm::vec<D, S>{width, height});
    }
  };

  using Rect2D = Rect<2, int32_t, uint32_t>;

  struct Viewport {
    float width;
    float height;
    float x = 0.0f;
    float y = 0.0f;
    float minDepth = 0.0f;
    float maxDepth = 1.0f;
  };
} // namespace kt::maths
