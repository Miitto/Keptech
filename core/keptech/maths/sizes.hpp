#pragma once

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include <glm/glm.hpp>

namespace kt::maths {
  template <typename O, typename S = O> struct Rect2D {
    O x;
    O y;
    S width;
    S height;

    Rect2D() = default;
    Rect2D(S width, S height, O x = 0, O y = 0) : x(x), y(y), width(width), height(height) {}
  };

  struct Viewport {
    float width = 0.f;
    float height = 0.f;
    float x = 0.0f;
    float y = 0.0f;
    float minDepth = 0.0f;
    float maxDepth = 1.0f;

    Viewport() = default;
    Viewport(float width, float height, float x = 0.0f, float y = 0.0f, float minDepth = 0.0f, float maxDepth = 1.0f)
        : width(width), height(height), x(x), y(y), minDepth(minDepth), maxDepth(maxDepth) {}
  };
} // namespace kt::maths
