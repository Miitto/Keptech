#include "keptech/core/components/camera.hpp"

#include "glm/ext/matrix_clip_space.hpp"

namespace keptech::components {
  void Camera::recalculateProjectionMatrix() {
    switch (projectionType) {
    case ProjectionType::Orthographic: {
      float zoom = params.orthographic.zoom;
      float aspect = params.common.aspectRatio;
      float left = -zoom * aspect * 0.5f;
      float right = zoom * aspect * 0.5f;
      float bottom = -zoom * 0.5f;
      float top = zoom * 0.5f;
      float near = params.common.planes.near;
      float far = params.common.planes.far;

      projectionMatrix = glm::ortho(left, right, bottom, top, near, far);
      break;
    }
    case ProjectionType::Perspective: {
      float fovY = params.perspective.fovY;
      float aspect = params.common.aspectRatio;
      float near = params.common.planes.near;
      float far = params.common.planes.far;
      projectionMatrix = glm::perspectiveLH_ZO(fovY, aspect, near, far);
      return;
    }
    case ProjectionType::PerspectiveInfinite: {
      float fovY = params.perspective.fovY;
      float aspect = params.common.aspectRatio;
      float near = params.common.planes.near;
      projectionMatrix = glm::infinitePerspectiveLH_ZO(fovY, aspect, near);
      return;
    }
    }
  }
} // namespace keptech::components
