#pragma once

#include "intersection.hpp"
#include "plane.hpp"
#include <array>
#include <glm/glm.hpp>

namespace kt::maths {
  struct Frustum {
    std::array<Plane, 6> planes; // Left, Right, Bottom, Top, Near, Far

    template <PlaneIntersectable T> [[nodiscard]] IntersectionType intersects(const T& obj) const {
      IntersectionType finalResult = IntersectionType::eWhole;
      for (const auto& plane : planes) {
        IntersectionType result = obj.intersects(plane);
        if (result == IntersectionType::eNone) {
          return IntersectionType::eNone;
        } else if (result == IntersectionType::ePartial) {
          finalResult = IntersectionType::ePartial;
        }
      }
      return finalResult;
    }

    constexpr static Frustum fromViewProjectionMatrix(const glm::mat4& vpMatrix) {
      Frustum frustum = {};

      glm::vec3 x = {vpMatrix[0][0], vpMatrix[1][0], vpMatrix[2][0]};
      glm::vec3 y = {vpMatrix[0][1], vpMatrix[1][1], vpMatrix[2][1]};
      glm::vec3 z = {vpMatrix[0][2], vpMatrix[1][2], vpMatrix[2][2]};
      glm::vec3 w = {vpMatrix[0][3], vpMatrix[1][3], vpMatrix[2][3]};

      float xd = vpMatrix[3][0];
      float yd = vpMatrix[3][1];
      float zd = vpMatrix[3][2];
      float d = vpMatrix[3][3];

      frustum.planes[0] = Plane(w - x, d - xd);
      frustum.planes[1] = Plane(w + x, d + xd);
      frustum.planes[2] = Plane(w - y, d - yd);
      frustum.planes[3] = Plane(w + y, d + yd);
      frustum.planes[4] = Plane(w - z, d - zd);
      frustum.planes[5] = Plane(w + z, d + zd);

      return frustum;
    }
  };
} // namespace kt::maths
