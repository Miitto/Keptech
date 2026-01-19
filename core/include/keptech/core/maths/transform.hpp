#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace keptech::maths {
  class Transform {
  public:
    [[nodiscard]] glm::vec3& pos() { return position; }
    [[nodiscard]] const glm::vec3& pos() const { return position; }
    [[nodiscard]] glm::quat& rot() { return rotation; }
    [[nodiscard]] const glm::quat& rot() const { return rotation; }
    [[nodiscard]] glm::vec3& scale() { return _scale; }
    [[nodiscard]] const glm::vec3& scale() const { return _scale; }

    Transform& translate(const glm::vec3& delta) {
      position += delta;
      return *this;
    }

    Transform& rotate(const glm::quat& delta) {
      rotation = delta * rotation;
      return *this;
    }

    Transform& lookAt(const glm::vec3& target, const glm::vec3& up) {
      rotation = glm::quatLookAt(glm::normalize(target - position), up);
      return *this;
    }

    Transform& resize(const glm::vec3& factor) {
      _scale *= factor;
      return *this;
    }

    Transform& setPosition(const glm::vec3& newPosition) {
      position = newPosition;
      return *this;
    }
    Transform& setRotation(const glm::quat& newRotation) {
      rotation = newRotation;
      return *this;
    }
    Transform& setScale(const glm::vec3& newScale) {
      _scale = newScale;
      return *this;
    }

    Transform& apply(const Transform& other) {
      position += other.position;
      rotation = other.rotation * rotation;
      _scale *= other._scale;
      return *this;
    }

    [[nodiscard]] glm::mat4 toMatrix(bool viewMatrix = false) const {
      glm::mat4 t = glm::translate(glm::mat4(1.0f), position);
      glm::mat4 r = glm::mat4(rotation);
      // Adjust for LH-Y up
      glm::mat4 s = glm::scale(glm::mat4(1.0f),
                               _scale * glm::vec3(1, viewMatrix ? -1 : 1, 1));
      return t * r * s;
    }

    explicit operator glm::mat4() const { return toMatrix(); }

  private:
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 _scale = glm::vec3(1.0f, 1.0f, 1.0f);
  };
} // namespace keptech::maths
