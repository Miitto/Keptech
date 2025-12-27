#pragma once

#include "keptech/core/bitflag.hpp"
#include "keptech/core/macros.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace keptech::core::cameras {
  struct Uniforms {
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 viewProjection;

    glm::mat4 inverseProjection;
    glm::mat4 inverseView;
    glm::mat4 inverseViewProjection;
  };

  enum class CameraMatrixFlags : uint8_t {
    Projection = BIT(0),
    View = BIT(1),
  };
} // namespace keptech::core::cameras

DEFINE_BITFLAG_ENUM_OPERATORS(keptech::core::cameras::CameraMatrixFlags)

namespace keptech::core::cameras {
  class Camera {
  public:
    Camera() = default;
    Camera(const Camera&) = default;
    Camera(Camera&&) = default;
    Camera& operator=(const Camera&) = default;
    Camera& operator=(Camera&&) = default;
    virtual ~Camera() = default;

    Camera& translate(const glm::vec3& deltaPosition) {
      position += deltaPosition;
      return *this;
    }

    Camera& rotate(const glm::quat& deltaRotation) {
      rotation = deltaRotation * rotation;
      return *this;
    }

    Camera& setPosition(const glm::vec3& newPosition) {
      position = newPosition;
      return *this;
    }

    Camera& setRotation(const glm::quat& newRotation) {
      rotation = newRotation;
      return *this;
    }

    [[nodiscard]] glm::vec3 getPosition() const { return position; }

    [[nodiscard]] glm::quat getRotation() const { return rotation; }

    Uniforms& getUniforms() { return uniforms; }
    [[nodiscard]] const Uniforms& getUniforms() const { return uniforms; }

    [[nodiscard]] bool isDirty() const { return dirty.any(); }
    [[nodiscard]] bool isProjectionDirty() const {
      return dirty.has(CameraMatrixFlags::Projection);
    }
    [[nodiscard]] bool isViewDirty() const {
      return dirty.has(CameraMatrixFlags::View);
    }
    void recalculate() {
      remakeProjectionMatrix();
      remakeViewMatrix();

      uniforms.viewProjection = uniforms.projection * uniforms.view;
      uniforms.inverseView = glm::inverse(uniforms.view);
      uniforms.inverseProjection = glm::inverse(uniforms.projection);
      uniforms.inverseViewProjection = glm::inverse(uniforms.viewProjection);
    }

    virtual void onViewportResize(int newWidth, int newHeight) = 0;

  protected:
    void remakeViewMatrix();
    virtual void remakeProjectionMatrix() = 0;

    core::Bitflag<CameraMatrixFlags> dirty{CameraMatrixFlags::Projection |
                                           CameraMatrixFlags::View};

    glm::vec3 position{};
    glm::quat rotation{};
    Uniforms uniforms{};
  };
} // namespace keptech::core::cameras
