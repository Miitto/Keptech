#pragma once

#include "intersection.hpp"
#include "plane.hpp"
#include <array>

namespace keptech::maths {
  struct Frustum {
    std::array<Plane, 6> planes; // Left, Right, Bottom, Top, Near, Far

    template <PlaneIntersectable T>
    [[nodiscard]] IntersectionType intersects(const T& obj) const {
      IntersectionType finalResult = IntersectionType::eWhole;
      for (const auto& plane : planes) {
        IntersectionType result = obj.inPlane(plane);
        if (result == IntersectionType::eNone) {
          return IntersectionType::eNone;
        } else if (result == IntersectionType::ePartial) {
          finalResult = IntersectionType::ePartial;
        }
      }
      return finalResult;
    }

    constexpr static Frustum
    fromViewProjectionMatrix(const glm::mat4& vpMatrix) {
      Frustum frustum = {};

      // Left plane
      frustum.planes[0].normal = glm::vec3(vpMatrix[0][3] + vpMatrix[0][0],
                                           vpMatrix[1][3] + vpMatrix[1][0],
                                           vpMatrix[2][3] + vpMatrix[2][0]);
      frustum.planes[0].distance = vpMatrix[3][3] + vpMatrix[3][0];

      // Right plane
      frustum.planes[1].normal = glm::vec3(vpMatrix[0][3] - vpMatrix[0][0],
                                           vpMatrix[1][3] - vpMatrix[1][0],
                                           vpMatrix[2][3] - vpMatrix[2][0]);
      frustum.planes[1].distance = vpMatrix[3][3] - vpMatrix[3][0];

      // Bottom plane
      frustum.planes[2].normal = glm::vec3(vpMatrix[0][3] + vpMatrix[0][1],
                                           vpMatrix[1][3] + vpMatrix[1][1],
                                           vpMatrix[2][3] + vpMatrix[2][1]);
      frustum.planes[2].distance = vpMatrix[3][3] + vpMatrix[3][1];

      // Top plane
      frustum.planes[3].normal = glm::vec3(vpMatrix[0][3] - vpMatrix[0][1],
                                           vpMatrix[1][3] - vpMatrix[1][1],
                                           vpMatrix[2][3] - vpMatrix[2][1]);
      frustum.planes[3].distance = vpMatrix[3][3] - vpMatrix[3][1];

      // Near plane
      frustum.planes[4].normal = glm::vec3(vpMatrix[0][3] + vpMatrix[0][2],
                                           vpMatrix[1][3] + vpMatrix[1][2],
                                           vpMatrix[2][3] + vpMatrix[2][2]);
      frustum.planes[4].distance = vpMatrix[3][3] + vpMatrix[3][2];

      // Far plane
      frustum.planes[5].normal = glm::vec3(vpMatrix[0][3] - vpMatrix[0][2],
                                           vpMatrix[1][3] - vpMatrix[1][2],
                                           vpMatrix[2][3] - vpMatrix[2][2]);

      return frustum;
    }
  };
} // namespace keptech::maths
