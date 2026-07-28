#pragma once

#include <keptech/components/camera.hpp>

namespace kt {
  namespace ecs {
    class Entity;
  }
  namespace maths {
    struct Frustum;
  }
} // namespace kt

namespace kt::rdr {
  struct Buffers;

  namespace passes {
    kt::maths::Frustum writeCameraData(const Buffers& buffers, const ecs::Entity cameraEntity, const glm::vec2& framebufferSize,
                                       size_t index);
  }
} // namespace kt::rdr