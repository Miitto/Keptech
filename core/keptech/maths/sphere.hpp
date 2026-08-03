#pragma once

#include "intersection.hpp"
#include "plane.hpp"
#include <keptech/core/winfix.h>

namespace kt::maths {
  struct Sphere;

  template <class T>
  concept SphereIntersectable = requires(const T& obj) {
    { obj.inSphere(std::declval<Sphere>()) } -> std::same_as<IntersectionType>;
  };

  struct Sphere {
    glm::vec3 center;
    float radius;

    template <SphereIntersectable T> [[nodiscard]] IntersectionType intersects(const T& obj) const { return obj.inSphere(*this); }

    [[nodiscard]] IntersectionType intersects(const Plane& plane) const {
      float dist = plane.getSignedDistance(center);
      if (dist < -radius) {
        return IntersectionType::eNone;
      } else if (dist > radius) {
        return IntersectionType::eWhole;
      } else {
        return IntersectionType::ePartial;
      }
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

    [[nodiscard]] Sphere apply(const glm::mat4& transform) const {
      glm::vec3 newCenter = glm::vec3(transform * glm::vec4(center, 1.f));
      float maxScale =
          std::max({glm::length(glm::vec3(transform[0])), glm::length(glm::vec3(transform[1])), glm::length(glm::vec3(transform[2]))});
      return Sphere{.center = newCenter, .radius = radius * maxScale};
    }
  };

  static_assert(SphereIntersectable<Sphere>, "Sphere does not satisfy SphereIntersectable concept");
  static_assert(PlaneIntersectable<Sphere>, "Sphere does not satisfy PlaneIntersectable concept");
} // namespace kt::maths
