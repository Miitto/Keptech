#pragma once

#include "intersection.hpp"
#include <glm/glm.hpp>

namespace kt::maths {
  struct Plane;

  template <class T>
  concept PlaneIntersectable = requires(const T& obj, float dist, const Plane& plane) {
    { obj.intersects(plane) } -> std::same_as<IntersectionType>;
  };

  struct Plane {
    glm::vec3 normal;
    float distance;

    Plane() = default;
    Plane(glm::vec3 normal, float distance) : normal(normal), distance(distance) {
      float length = glm::length(normal);
      this->normal /= length;
      this->distance /= length;
    }

    [[nodiscard]] float getSignedDistance(glm::vec3 point) const { return glm::dot(normal, point) + distance; }

    template <PlaneIntersectable T> [[nodiscard]] IntersectionType intersects(const T& obj) const { return obj.inPlane(*this); }
  };
} // namespace kt::maths
