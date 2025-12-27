#include "keptech/core/cameras/camera.hpp"

#include "keptech/core/components/transform.hpp"

#include <keptech/ecs/ecs.hpp>

namespace keptech::core::cameras {
  bool Camera::remakeViewMatrix() {
    if (attachedEntity != ecs::INVALID_ENTITY_HANDLE) {
      auto& ecs = ecs::ECS::get();
      auto* entity = ecs.getEntity(attachedEntity);

      if (entity) {
        auto* transform =
            ecs.getComponent<components::Transform>(attachedEntity);

        if (transform) {
          transform->recalculateGlobalTransform();

          auto copy = transform->global;
          copy.translate(position);
          copy.rotate(rotation);

          uniforms.view = glm::inverse(copy.toMatrix());
          return true;
        }
      }
    }

    if (!dirty.has(CameraMatrixFlags::View)) {
      return false;
    }

    glm::mat4 translation = glm::translate(glm::mat4(1.0f), -position);
    glm::mat4 rotationMat = glm::mat4_cast(glm::conjugate(rotation));

    uniforms.view = rotationMat * translation;

    dirty.clear(CameraMatrixFlags::View);
    return true;
  }

  bool Camera::remakeProjectionMatrix() {
    if (!dirty.has(CameraMatrixFlags::Projection)) {
      return false;
    }

    switch (projectionType) {
    case ProjectionType::Orthographic: {
      auto max = viewport.max();

      uniforms.projection =
          glm::ortho(viewport.offset.x, max.x, viewport.offset.y, max.y,
                     nearPlane, farPlane);
      break;
    }
    case ProjectionType::Perspective: {
      uniforms.projection = glm::perspectiveFovRH_ZO(
          fovY, viewport.size.x, viewport.size.y, nearPlane, farPlane);
      break;
    }
    }

    return true;
  }
} // namespace keptech::core::cameras
