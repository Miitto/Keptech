#include "keptech/core/maths/transform.hpp"

namespace keptech::maths {
  Transform& Transform::apply(const Transform& other) {
    position = (other.rotation * position) + other.position;
    rotation = other.rotation * rotation;
    _scale *= other._scale;
    return *this;
  }

  glm::mat4 Transform::toMatrix(bool viewMatrix) const {
    glm::mat4 t = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 r = glm::mat4(rotation);
    // Adjust for LH-Y up
    glm::mat4 s = glm::scale(glm::mat4(1.0f),
                             _scale * glm::vec3(1, viewMatrix ? -1 : 1, 1));
    return t * r * s;
  }
} // namespace keptech::maths
