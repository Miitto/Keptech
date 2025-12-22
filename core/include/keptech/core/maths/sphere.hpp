#pragma once

#include "intersection.hpp"
#include "plane.hpp"

namespace keptech::maths {
  struct Sphere;

  template <class T>
  concept SphereIntersectable = requires(const T& obj) {
    { obj.inSphere(std::declval<Sphere>()) } -> std::same_as<IntersectionType>;
  };

  struct Sphere {
    glm::vec3 center;
    float radius;

    template <SphereIntersectable T>
    [[nodiscard]] IntersectionType intersects(const T& obj) const {
      return obj.inSphere(*this);
    }

    [[nodiscard]] IntersectionType inPlane(float dist) const {
      if (dist > radius) {
        return IntersectionType::eNone;
      } else if (dist < -radius) {
        return IntersectionType::eWhole;
      } else {
        return IntersectionType::ePartial;
      }
    }

    [[nodiscard]] IntersectionType intersects(const Plane& plane) const {
      float dist = plane.getSignedDistance(center);
      return inPlane(dist);
    }

    [[nodiscard]] IntersectionType inSphere(const Sphere& other) const {
      glm::vec3 to = other.center - center;
      float distSq = glm::dot(to, to);
      float radiusSum = radius + other.radius;
      if (distSq > radiusSum * radiusSum) {
        return IntersectionType::eNone;
      } else if (distSq < (radius - other.radius) * (radius - other.radius)) {
        return IntersectionType::eWhole;
      } else {
        return IntersectionType::ePartial;
      }
    }
  };

  static_assert(SphereIntersectable<Sphere>,
                "Sphere does not satisfy SphereIntersectable concept");
  static_assert(PlaneIntersectable<Sphere>,
                "Sphere does not satisfy PlaneIntersectable concept");
} // namespace keptech::maths
