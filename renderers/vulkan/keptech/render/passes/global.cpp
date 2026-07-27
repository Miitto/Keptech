#include "global.hpp"

#include "buffers.hpp"
#include "constants.hpp"
#include "keptech/maths/maths.hpp"
#include "profile.hpp"
#include <keptech/components/camera.hpp>
#include <keptech/components/transform.hpp>
#include <keptech/maths/frustum.hpp>

namespace kt::vkh::passes {
  kt::maths::Frustum writeCameraData(const Buffers& buffers, const ecs::Entity cameraEntity, const glm::vec2& framebufferSize,
                                     size_t index) {
    KT_PROFILE_FUNCTION
    auto [camT, cam] = cameraEntity.getComponents<components::Transform, components::Camera>();

    cam.recalculateProjectionMatrix();
    auto projection = cam.getProjectionMatrix();
    auto invView = camT.getGlobal();
    auto viewMat = glm::inverse(invView);
    auto invProj = glm::inverse(projection);

    auto viewProj = projection * viewMat;
    auto invViewProj = glm::inverse(viewProj);

    auto frustum = kt::maths::Frustum::fromViewProjectionMatrix(viewProj);

    components::Camera::Uniforms camUniforms{
        .projectionMatrix = projection,
        .viewMatrix = viewMat,
        .viewProjectionMatrix = viewProj,
        .invProjectionMatrix = invProj,
        .invViewMatrix = invView,
        .invViewProjectionMatrix = invViewProj,
        .frustum = frustum,
        .viewportSize = {framebufferSize.x, framebufferSize.y},
    };

    size_t sizePerCamera = maths::roundToAlignment(sizeof(components::Camera::Uniforms), limits::minUniformBufferOffsetAlignment);
    size_t offset = index * sizePerCamera;

    memcpy(buffers.camera.mapping() + offset, &camUniforms, sizeof(components::Camera::Uniforms));

    return frustum;
  }
} // namespace kt::vkh::passes