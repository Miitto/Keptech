#pragma once

#include "keptech/core/bitflag.hpp"
#include "keptech/core/macros.hpp"
#include "keptech/core/maths/extent.hpp"
#include "keptech/ecs/base.hpp"
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
    enum class ProjectionType : uint8_t { Orthographic, Perspective };

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
      bool proj = remakeProjectionMatrix();
      bool view = remakeViewMatrix();

      if (view || proj) {
        uniforms.viewProjection = uniforms.projection * uniforms.view;
        uniforms.inverseViewProjection = glm::inverse(uniforms.viewProjection);
      }
      if (view)
        uniforms.inverseView = glm::inverse(uniforms.view);
      if (proj)
        uniforms.inverseProjection = glm::inverse(uniforms.projection);
    }

    [[nodiscard]] const maths::Extent2Df& getViewport() const {
      return viewport;
    }
    Camera& setViewport(const maths::Extent2Df& newViewport) {
      viewport = newViewport;
      dirty.set(CameraMatrixFlags::Projection);
      return *this;
    }

    [[nodiscard]] const maths::Extent2Du& getScissor() const { return scissor; }
    Camera& setScissor(const maths::Extent2Du& newScissor) {
      scissor = newScissor;
      return *this;
    }

    Camera& attachToEntity(ecs::EntityHandle entity) {
      attachedEntity = entity;
      return *this;
    }
    Camera& detachFromEntity() {
      attachedEntity = ecs::INVALID_ENTITY_HANDLE;
      return *this;
    }
    [[nodiscard]] ecs::EntityHandle getAttachedEntity() const {
      return attachedEntity;
    }

    Camera& setProjectionType(ProjectionType type) {
      projectionType = type;
      dirty.set(CameraMatrixFlags::Projection);
      return *this;
    }
    [[nodiscard]] ProjectionType getProjectionType() const {
      return projectionType;
    }

    Camera& setFovY(float fovYDegrees) {
      fovY = glm::radians(fovYDegrees);
      dirty.set(CameraMatrixFlags::Projection);
      return *this;
    }

    Camera& setPriority(float newPriority) {
      priority = newPriority;
      return *this;
    }
    [[nodiscard]] float getPriority() const { return priority; }

  protected:
    bool remakeViewMatrix();
    bool remakeProjectionMatrix();

    core::Bitflag<CameraMatrixFlags> dirty{CameraMatrixFlags::Projection |
                                           CameraMatrixFlags::View};

    ProjectionType projectionType{ProjectionType::Perspective};

    glm::vec3 position{};
    glm::quat rotation{};
    maths::Extent2Df viewport{};
    maths::Extent2Du scissor{};
    float nearPlane{0.1f};
    float farPlane{1000.0f};
    float fovY{60.0f};
    Uniforms uniforms{};
    ecs::EntityHandle attachedEntity{ecs::INVALID_ENTITY_HANDLE};
    float priority{1.0f};
  };
} // namespace keptech::core::cameras
