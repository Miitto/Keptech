#pragma once

#include "intersection.hpp"

namespace keptech::maths {
  struct Plane;

  template <class T>
  concept PlaneIntersectable =
      requires(const T& obj, float dist, const Plane& plane) {
        { obj.inPlane(dist) } -> std::same_as<IntersectionType>;
        { obj.intersects(plane) } -> std::same_as<IntersectionType>;
      };

  struct Plane {
    glm::vec3 normal;
    float distance;

    [[nodiscard]] float getSignedDistance(glm::vec3 point) const {
      return glm::dot(normal, point) + distance;
    }

    template <PlaneIntersectable T>
    [[nodiscard]] IntersectionType intersects(const T& obj) const {
      return obj.inPlane(*this);
    }
  };
} // namespace keptech::maths
