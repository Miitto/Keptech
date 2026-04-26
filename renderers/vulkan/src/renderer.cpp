#include "keptech/vulkan/renderer.hpp"

#include "keptech/components/lights.hpp"
#include "keptech/vulkan/helpers/swapchain.hpp"
#include "macros.hpp"
#include <keptech/maths/maths.hpp>

#include "keptech/vulkan/constants.hpp"
#include "profile.hpp"
#include "setup/setup.hpp"
#include "vk-logger.hpp"
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/components/camera.hpp>
#include <keptech/core/profile.hpp>
#include <keptech/core/window.hpp>
#include <keptech/rendering/structs.hpp>

namespace kt::vkh {
  static_assert(CRenderer<Renderer>, "Renderer does not satisfy CRenderer concept");

  constexpr VkDeviceSize NO_VERTEX_OFFSET = 0;

  void Renderer::debugUi() {
    ImGui::Begin("Debug View");

    auto camera = scene->getActiveCamera();
    auto& camT = camera.getComponents<components::Transform>();
    auto camPos = camT.getGlobal()[3];

    ImGui::Text("Camera Position: %.2f, %.2f, %.2f", camPos.x, camPos.y, camPos.z);

    ImGui::End();
  }

  namespace {
    void recalcGlobalTransforms(entt::registry& registry) {
      KT_PROFILE_FUNCTION
      auto view = registry.view<components::Transform>();
      for (auto [entity, transform] : view.each()) {
        transform.recalculateGlobalTransform();
      }
    }
  } // namespace

  void Renderer::render() {
    KT_PROFILE_FUNCTION
    VK_TRACE("Frame Start");
    startFrame();

    recalcGlobalTransforms(scene->getEcs());

    std::array<VkCommandBuffer, 3> cmdBuf{};
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m.frameInfo.perFrame->pools.graphics.pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 3,
    };
    VK_CHECK(vkAllocateCommandBuffers(m.vkcore.device.logical, &allocInfo, cmdBuf.data()), "Failed to allocate command buffer for frame");
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmdBuf[0], &beginInfo);
    {
      KT_VK_ZONE(m.tracyContext, cmdBuf[0], "Render Deferred");

      VK_TRACE("Camera Buffer Update");
      updateCameraBuffer(cmdBuf[0]);
      VK_TRACE("Objects Buffer Update");
      updateObjectsBuffer();
      VK_TRACE("Draw Deferred");
      drawDeferred(cmdBuf[0]);
      KT_VK_COLLECT(m.tracyContext, cmdBuf[0]);
    }
    vkEndCommandBuffer(cmdBuf[0]);
    VK_TRACE("Submit Deferred");
    submitDeferred(cmdBuf[0]);

    vkBeginCommandBuffer(cmdBuf[1], &beginInfo);
    {
      KT_VK_ZONE(m.tracyContext, cmdBuf[1], "Render Lights");
      VK_TRACE("Draw Lights");
      drawLights(cmdBuf[1]);
      KT_VK_COLLECT(m.tracyContext, cmdBuf[1]);
    }
    vkEndCommandBuffer(cmdBuf[1]);
    VK_TRACE("Submit Lights");
    submitLights(cmdBuf[1]);
    vkBeginCommandBuffer(cmdBuf[2], &beginInfo);
    {
      KT_VK_ZONE(m.tracyContext, cmdBuf[2], "Final Pass and UI");
      VK_TRACE("Post Processing");
      renderBloom(cmdBuf[2]);
      KT_VK_COLLECT(m.tracyContext, cmdBuf[2]);

#ifndef NDEBUG
      debugUi();
#endif
      renderImGui(cmdBuf[2]);
      VkImageMemoryBarrier2 barrier{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
          .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
          .dstAccessMask = VK_ACCESS_2_NONE,
          .oldLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
          .srcQueueFamilyIndex = m.vkcore.queues.graphics.index,
          .dstQueueFamilyIndex = m.vkcore.queues.present.index,
          .image = m.vkcore.swapchain.nImage(m.frameInfo.imageIndex),
          .subresourceRange =
              VkImageSubresourceRange{
                  .aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
                  .baseMipLevel = 0,
                  .levelCount = 1,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
      };

      VkDependencyInfo dependencyInfo{
          .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .imageMemoryBarrierCount = 1,
          .pImageMemoryBarriers = &barrier,
      };

      vkCmdPipelineBarrier2(cmdBuf[2], &dependencyInfo);

      KT_VK_COLLECT(m.tracyContext, cmdBuf[2]);
    }
    vkEndCommandBuffer(cmdBuf[2]);
    VK_TRACE("Present");
    endFrame(cmdBuf[2]);
  }

  void Renderer::updateCameraBuffer(VkCommandBuffer cmdBuf) {
    KT_PROFILE_FUNCTION
    KT_VK_ZONE(m.tracyContext, cmdBuf, "Update Camera Buffer");
    auto [camT, cam] = scene->getActiveCamera().getComponents<components::Transform, components::Camera>();

    cam.recalculateProjectionMatrix();
    auto projection = cam.getProjectionMatrix();
    auto invView = camT.getGlobal();
    auto viewMat = glm::inverse(invView);
    auto invProj = glm::inverse(projection);

    auto viewProj = projection * viewMat;
    auto invViewProj = glm::inverse(viewProj);

    components::Camera::Uniforms camUniforms{
        .projectionMatrix = projection,
        .viewMatrix = viewMat,
        .viewProjectionMatrix = viewProj,
        .invProjectionMatrix = invProj,
        .invViewMatrix = invView,
        .invViewProjectionMatrix = invViewProj,
        .viewportSize = {m.renderTargets.framebufferSize.x, m.renderTargets.framebufferSize.y},
    };

    size_t sizePerCamera = maths::roundToAlignment(sizeof(components::Camera::Uniforms), limits::minUniformBufferOffsetAlignment);
    size_t offset = m.frameInfo.index * sizePerCamera;

    memcpy(m.buffers.camera.mapping() + offset, &camUniforms, sizeof(components::Camera::Uniforms));
  }

  void Renderer::updateObjectsBuffer() {
    KT_PROFILE_FUNCTION
    auto& buf = fBufs().objects;
    auto& drawBuf = fBufs().drawCommands;
    auto view = scene->getEcs().view<components::Mesh, components::Transform>();

    std::vector<Renderer::GpuObject> gpuObjects;
    std::vector<VkDrawIndexedIndirectCommand> drawCommands;
    gpuObjects.reserve(view.size_hint());
    drawCommands.reserve(view.size_hint());

    for (const auto& [entity, mesh, transform] : view.each()) {
      uint32_t firstIndex = mesh.getRMesh().firstIndex;
      int32_t vertexOffset = static_cast<int32_t>(mesh.getRMesh().firstVertex);
      for (const auto& submesh : mesh.getSubmeshes()) {
        Renderer::GpuObject gpuObject{
            .model = transform.getGlobal(),
            .materialIndex = submesh.material.has_value() ? submesh.material.value() : ~0u,
        };
        gpuObjects.push_back(gpuObject);

        VkDrawIndexedIndirectCommand drawCommand{
            .indexCount = submesh.count,
            .instanceCount = 1,
            .firstIndex = firstIndex + submesh.start,
            .vertexOffset = vertexOffset,
            .firstInstance = static_cast<uint32_t>(gpuObjects.size() - 1),
        };
        drawCommands.push_back(drawCommand);
      }
    }

    const size_t objectRequiredSize = gpuObjects.size() * sizeof(Renderer::GpuObject);
    const size_t drawCommandRequiredSize = drawCommands.size() * sizeof(VkDrawIndexedIndirectCommand);

    if (objectRequiredSize > buf.buffer.size()) {
      buf.buffer.destroy(m.vkcore.allocator);
      VkBufferCreateInfo bufInfo{
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .size = objectRequiredSize,
          .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      };

      VmaAllocationCreateInfo allocInfo{
          .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
          .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      };

      auto res = AddressedAllocatedBuffer::create(m.vkcore.device.logical, m.vkcore.allocator, bufInfo, allocInfo, "Object Buffer");
      VK_ASSERT(res.has_value(), "Failed to create object buffer: {}", res.error());

      buf = {.buffer = res.value()};

      VK_DEBUG("Resized object buffer to fit {} objects", gpuObjects.size());
    }

    if (drawCommandRequiredSize > drawBuf.buffer.size()) {
      drawBuf.buffer.destroy(m.vkcore.allocator);
      VkBufferCreateInfo bufInfo{
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .size = drawCommandRequiredSize,
          .usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      };

      VmaAllocationCreateInfo allocInfo{
          .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
          .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      };

      auto res = AddressedAllocatedBuffer::create(m.vkcore.device.logical, m.vkcore.allocator, bufInfo, allocInfo, "Draw Command Buffer");
      VK_ASSERT(res.has_value(), "Failed to create draw command buffer: {}", res.error());

      drawBuf = {.buffer = res.value()};

      VK_DEBUG("Resized draw command buffer to fit {} draw commands", drawCommands.size());
    }

    if (!gpuObjects.empty()) {
      buf.overwrite(gpuObjects);
    }

    drawBuf.overwrite(drawCommands);
  }

  void Renderer::drawDeferred(VkCommandBuffer cmdBuf) {
    KT_PROFILE_FUNCTION
    KT_VK_ZONE(m.tracyContext, cmdBuf, "Draw Deferred");
    deferredToRenderable(cmdBuf);
    deferredBeginRendering(cmdBuf);

    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.deferred.pipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.deferred.layout, 0, 1,
                            &m.globalDescriptorSets.sets[m.frameInfo.index], 0, nullptr);
    setupViewportAndScissor(cmdBuf);

    vkCmdBindVertexBuffers(cmdBuf, 0, 1, &m.buffers.vertices.buffer.buffer, &NO_VERTEX_OFFSET);
    vkCmdBindIndexBuffer(cmdBuf, m.buffers.indices.buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    struct Addresses {
      VkDeviceAddress objectBufferAddress;
      VkDeviceAddress materialBufferAddress;
    } addresses{
        .objectBufferAddress = fBufs().objects.buffer.address,
        .materialBufferAddress = m.buffers.materials.buffer.address,
    };
    vkCmdPushConstants(cmdBuf, m.pipelines.deferred.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Addresses),
                       &addresses);

    auto& drawBuf = fBufs().drawCommands;
    vkCmdDrawIndexedIndirect(cmdBuf, drawBuf.buffer, 0, drawBuf.count, sizeof(VkDrawIndexedIndirectCommand));
    VK_TRACE("Drew {} objects in deferred pass", drawCount);

    vkCmdEndRendering(cmdBuf);

    deferredToShaderRead(cmdBuf);
  }

  void Renderer::submitDeferred(VkCommandBuffer cmdBuf) {
    VkSemaphoreSubmitInfo semInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = m.frameInfo.perFrame->deferredRenderFinishedSemaphore,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    };
    VkCommandBufferSubmitInfo cmdBufInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmdBuf,
    };
    VkSubmitInfo2 submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdBufInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &semInfo,
    };

    auto res = vkQueueSubmit2(m.vkcore.queues.graphics.queue, 1, &submitInfo, nullptr);
    VK_ASSERT(res == VK_SUCCESS, "Failed to submit deferred command buffer: {}", res);
  }

  void Renderer::drawLights(VkCommandBuffer cmdBuf) {
    KT_PROFILE_FUNCTION
    KT_VK_ZONE(m.tracyContext, cmdBuf, "Draw Lights");
    lightsToRenderable(cmdBuf);

    auto lightInfo = updatePointLightsBuffer();
    drawPointLightShadowMaps(cmdBuf, lightInfo);
    drawPointLights(cmdBuf);

    seperatedLightsToShaderRead(cmdBuf);

    renderSsao(cmdBuf);

    combineLights(cmdBuf);

    combinedLightToShaderRead(cmdBuf);
  }

  std::vector<Renderer::LightRenderInfo> Renderer::updatePointLightsBuffer() {
    KT_PROFILE_FUNCTION
    constexpr std::array<glm::vec3, 6> directions{
        glm::vec3{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1},
    };

    constexpr std::array<glm::vec3, 6> upDirections{
        glm::vec3{0, 1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {0, 1, 0}, {0, 1, 0},
    };

    auto& lightBuf = fBufs().pointLights;
    auto& shadowBuf = fBufs().shadowMatrices;

    auto shadowView = scene->getEcs().view<components::Transform, components::PointLight>();
    std::vector<Renderer::GpuPointLight> lights;
    std::vector<glm::mat4> shadowMatrices;
    std::vector<LightRenderInfo> shadowMaps;
    lights.reserve(shadowView.size_hint());
    shadowMatrices.reserve(shadowView.size_hint() * 6);
    shadowMaps.reserve(shadowView.size_hint());
    for (auto [entity, transform, pointLight] : shadowView.each()) {
      if (pointLight.shadowMap.getIndex() == ~0u) {
        VkImageCreateInfo shadowMapCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
            .imageType = VkImageType::VK_IMAGE_TYPE_2D,
            .format = m.formats.render.depth,
            .extent =
                VkExtent3D{
                    .width = constants::SHADOW_MAP_SIZE,
                    .height = constants::SHADOW_MAP_SIZE,
                    .depth = 1,
                },
            .mipLevels = 1,
            .arrayLayers = 6,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        };
        VmaAllocationCreateInfo shadowMapAllocInfo{
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        };
        VkImageViewCreateInfo shadowMapViewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
            .format = m.formats.render.depth,
            .components =
                {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 6,
                },
        };

        auto shadowMapRes = AllocatedImage::create(m.vkcore.allocator, m.vkcore.device.logical, shadowMapCreateInfo, shadowMapAllocInfo,
                                                   shadowMapViewCreateInfo, false, "PointLightShadowMap");
        VK_ASSERT(shadowMapRes.has_value(), "Failed to create shadow map for point light: {}", shadowMapRes.error());
        auto index = m.nextTextureIndex++;
        Texture shadowMap(Texture::Type::eCube, glm::ivec3{constants::SHADOW_MAP_SIZE, constants::SHADOW_MAP_SIZE, 1}, 1,
                          m.formats.render.depth, index, shadowMapRes.value());
        m.loadedTextures.push_back(shadowMapRes.value());
        for (auto& frame : m.vkcore.perFrame) {
          frame.texToUpdate.emplace_back(shadowMapRes.value(), index);
        }
        pointLight.shadowMap = shadowMap;
      }

      glm::vec3 shadowCenter = transform.getGlobal()[3];

      GpuPointLight gpuLight{
          .position = shadowCenter,
          .radius = pointLight.radius,
          .color = pointLight.color * pointLight.intensity,
          .shadowMapIndex = pointLight.shadowMap.getIndex(),
      };
      lights.push_back(gpuLight);

      glm::mat4 shadowProj = glm::perspectiveLH_ZO(glm::radians(90.f), 1.f, 0.1f, pointLight.radius);
      std::array<glm::mat4, 6> shadowViews = {
          glm::lookAtLH(shadowCenter, shadowCenter + directions[0], upDirections[0]),
          glm::lookAtLH(shadowCenter, shadowCenter + directions[1], upDirections[1]),
          glm::lookAtLH(shadowCenter, shadowCenter + directions[2], upDirections[2]),
          glm::lookAtLH(shadowCenter, shadowCenter + directions[3], upDirections[3]),
          glm::lookAtLH(shadowCenter, shadowCenter + directions[4], upDirections[4]),
          glm::lookAtLH(shadowCenter, shadowCenter + directions[5], upDirections[5]),
      };

      shadowMatrices.emplace_back(shadowProj * shadowViews[0]);
      shadowMatrices.emplace_back(shadowProj * shadowViews[1]);
      shadowMatrices.emplace_back(shadowProj * shadowViews[2]);
      shadowMatrices.emplace_back(shadowProj * shadowViews[3]);
      shadowMatrices.emplace_back(shadowProj * shadowViews[4]);
      shadowMatrices.emplace_back(shadowProj * shadowViews[5]);

      shadowMaps.emplace_back(pointLight.shadowMap);
    }

    const size_t lightDataSize = lights.size() * sizeof(Renderer::GpuPointLight);
    const size_t shadowMatrixDataSize = shadowMatrices.size() * sizeof(glm::mat4);

    if (lightDataSize > lightBuf.buffer.size()) {
      lightBuf.buffer.destroy(m.vkcore.allocator);
      VkBufferCreateInfo bufInfo{
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .size = lightDataSize,
          .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      };

      VmaAllocationCreateInfo allocInfo{
          .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
          .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      };

      auto res = AddressedAllocatedBuffer::create(m.vkcore.device.logical, m.vkcore.allocator, bufInfo, allocInfo, "Point Light Buffer");
      VK_ASSERT(res.has_value(), "Failed to create point light buffer: {}", res.error());

      lightBuf = {.buffer = res.value()};
      VK_DEBUG("Resized point light buffer to fit {} lights", lights.size());
    }

    if (shadowMatrixDataSize > shadowBuf.buffer.size()) {
      shadowBuf.buffer.destroy(m.vkcore.allocator);
      VkBufferCreateInfo bufInfo{
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .size = shadowMatrixDataSize,
          .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      };

      VmaAllocationCreateInfo allocInfo{
          .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
          .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      };

      auto res = AddressedAllocatedBuffer::create(m.vkcore.device.logical, m.vkcore.allocator, bufInfo, allocInfo, "Shadow Matrix Buffer");
      VK_ASSERT(res.has_value(), "Failed to create shadow matrix buffer: {}", res.error());

      shadowBuf = {.buffer = res.value()};
      VK_DEBUG("Resized shadow matrix buffer to fit {} matrices", shadowMatrices.size());
    }

    if (!lights.empty()) {
      lightBuf.overwrite(lights);
    }
    if (!shadowMatrices.empty()) {
      shadowBuf.overwrite(shadowMatrices);
    }

    return shadowMaps;
  }

  void Renderer::drawPointLightShadowMaps(VkCommandBuffer cmdBuf, const std::vector<LightRenderInfo>& lightInfo) {
    KT_PROFILE_FUNCTION
    KT_VK_ZONE(m.tracyContext, cmdBuf, "Draw Point Light Shadow Maps");

    struct Addresses {
      VkDeviceAddress objectBufferAddress;
      VkDeviceAddress lightBufferAddress;
      VkDeviceAddress shadowMatrixBufferAddress;
    } addresses{
        .objectBufferAddress = fBufs().objects.buffer.address,
        .lightBufferAddress = fBufs().pointLights.buffer.address,
        .shadowMatrixBufferAddress = fBufs().shadowMatrices.buffer.address,
    };

    vkCmdPushConstants(cmdBuf, m.pipelines.pointLightShadows.layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Addresses),
                       &addresses);

    auto& drawBuf = fBufs().drawCommands;
    for (uint32_t i = 0; i < lightInfo.size(); i++) {
      KT_PROFILE_SCOPE("Shadow Map Draw Calls");
      KT_VK_ZONE(m.tracyContext, cmdBuf, "Draw Shadow Casters");

      shadowMapToRenderable(cmdBuf, lightInfo[i].shadowMap.getImage(), true);
      shadowMapBeginRendering(cmdBuf, lightInfo[i].shadowMap.getImage(), true);

      vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.pointLightShadows.pipeline);
      vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.pointLightShadows.layout, 0, 1,
                              &m.globalDescriptorSets.sets[m.frameInfo.index], 0, nullptr);

      setupCustomViewportAndScissor(cmdBuf, {0, 0}, {constants::SHADOW_MAP_SIZE, constants::SHADOW_MAP_SIZE});
      vkCmdBindVertexBuffers(cmdBuf, 0, 1, &m.buffers.vertices.buffer.buffer, &NO_VERTEX_OFFSET);
      vkCmdBindIndexBuffer(cmdBuf, m.buffers.indices.buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

      uint32_t shadowMatrixIndex = i * 6;

      struct Indices {
        uint32_t lightIndex;
        uint32_t shadowMatrixIndex;
      } indices{
          .lightIndex = i,
          .shadowMatrixIndex = shadowMatrixIndex,
      };

      vkCmdPushConstants(cmdBuf, m.pipelines.pointLightShadows.layout,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Addresses),
                         sizeof(Indices), &indices);

      vkCmdDrawIndexedIndirect(cmdBuf, drawBuf.buffer, 0, drawBuf.count, sizeof(VkDrawIndexedIndirectCommand));
      VK_TRACE("Drew {} objects for shadow map of light {}", drawBuf.count, i);

      vkCmdEndRendering(cmdBuf);
      shadowMapToShaderRead(cmdBuf, lightInfo[i].shadowMap.getImage(), true);
    }
  }

  void Renderer::drawPointLights(VkCommandBuffer cmdBuf) {
    KT_PROFILE_FUNCTION
    KT_VK_ZONE(m.tracyContext, cmdBuf, "Draw Point Lights");

    seperatedLightsBeginRendering(cmdBuf);

    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.deferredPointLight.pipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.deferredPointLight.layout, 0, 1,
                            &m.globalDescriptorSets.sets[m.frameInfo.index], 0, nullptr);

    setupViewportAndScissor(cmdBuf);

    struct Addresses {
      VkDeviceAddress lightBufferAddress;
    } addresses{
        .lightBufferAddress = fBufs().pointLights.buffer.address,
    };
    vkCmdPushConstants(cmdBuf, m.pipelines.deferredPointLight.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(Addresses), &addresses);

    vkCmdDraw(cmdBuf, 36, fBufs().pointLights.count, 0, 0);

    vkCmdEndRendering(cmdBuf);
  }

  void Renderer::renderSsao(VkCommandBuffer cmdBuf) {
    KT_PROFILE_FUNCTION
    KT_VK_ZONE(m.tracyContext, cmdBuf, "Render SSAO");
    {
      KT_VK_ZONE(m.tracyContext, cmdBuf, "SSAO Pass");
      colorImageToRenderable(cmdBuf, m.renderTargets.lights.ssaoResult);
      colorImageBeginRendering(cmdBuf, m.renderTargets.lights.ssaoResult);

      vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.ssao.pipeline);
      vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.ssao.layout, 0, 1,
                              &m.globalDescriptorSets.sets[m.frameInfo.index], 0, nullptr);

      setupViewportAndScissor(cmdBuf);

      vkCmdDraw(cmdBuf, 3, 1, 0, 0);

      vkCmdEndRendering(cmdBuf);

      colorImageToShaderRead(cmdBuf, m.renderTargets.lights.ssaoResult);
    }

    {
      KT_VK_ZONE(m.tracyContext, cmdBuf, "SSAO Blur Pass");
      colorImageToRenderable(cmdBuf, m.renderTargets.lights.ssaoBlur);
      colorImageBeginRendering(cmdBuf, m.renderTargets.lights.ssaoBlur, false);

      vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.ssaoBlur.pipeline);
      vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.ssaoBlur.layout, 0, 1,
                              &m.globalDescriptorSets.sets[m.frameInfo.index], 0, nullptr);

      setupViewportAndScissor(cmdBuf);

      glm::vec2 texelSize = 1.f / glm::vec2(m.renderTargets.framebufferSize);
      vkCmdPushConstants(cmdBuf, m.pipelines.ssaoBlur.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(glm::vec2), &texelSize);

      vkCmdDraw(cmdBuf, 3, 1, 0, 0);
      vkCmdEndRendering(cmdBuf);

      colorImageToShaderRead(cmdBuf, m.renderTargets.lights.ssaoBlur);
    }
  }

  void Renderer::combineLights(VkCommandBuffer cmdBuf) {
    KT_PROFILE_FUNCTION
    KT_VK_ZONE(m.tracyContext, cmdBuf, "Combine Lights");
    combinedLightBeginRendering(cmdBuf);

    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.deferredCombine.pipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.deferredCombine.layout, 0, 1,
                            &m.globalDescriptorSets.sets[m.frameInfo.index], 0, nullptr);

    setupViewportAndScissor(cmdBuf);

    vkCmdDraw(cmdBuf, 3, 1, 0, 0);

    vkCmdEndRendering(cmdBuf);
  }

  void Renderer::submitLights(VkCommandBuffer cmdBuf) {
    VkSemaphoreSubmitInfo semInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = m.frameInfo.perFrame->lightsFinished,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    };
    VkCommandBufferSubmitInfo cmdBufInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmdBuf,
    };
    VkSemaphoreSubmitInfo waitSemInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = m.frameInfo.perFrame->deferredRenderFinishedSemaphore,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    };
    VkSubmitInfo2 submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &waitSemInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdBufInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &semInfo,
    };
    auto res = vkQueueSubmit2(m.vkcore.queues.graphics.queue, 1, &submitInfo, nullptr);
    VK_ASSERT(res == VK_SUCCESS, "Failed to submit light command buffer: {}", res);
  }

  void Renderer::renderBloom(VkCommandBuffer cmdBuf) {
    KT_PROFILE_FUNCTION
    KT_VK_ZONE(m.tracyContext, cmdBuf, "Render Bloom");
    glm::vec2 size = m.renderTargets.framebufferSize;

    static float filterRadius = 0.005f;

    uint32_t sampleIndex = constants::BloomSource;

    {
      KT_VK_ZONE(m.tracyContext, cmdBuf, "Bloom Downsample Passes");
      for (size_t i = 0; i < constants::BLOOM_MIP_LEVELS; i++) {
        auto& mip = m.renderTargets.bloomMips[i];
        colorImageToRenderable(cmdBuf, mip.image);
        colorImageBeginRendering(cmdBuf, mip.image);

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.bloomDownsample.pipeline);
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.bloomDownsample.layout, 0, 1,
                                &m.globalDescriptorSets.sets[m.frameInfo.index], 0, nullptr);

        setupCustomViewportAndScissor(cmdBuf, {}, {mip.size.x, mip.size.y});

        struct PushConstants {
          glm::vec2 texelSize;
          uint32_t level;
        } pushConstants{
            .texelSize = 1.f / size,
            .level = sampleIndex,
        };
        vkCmdPushConstants(cmdBuf, m.pipelines.bloomDownsample.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(PushConstants), &pushConstants);

        vkCmdDraw(cmdBuf, 3, 1, 0, 0);

        vkCmdEndRendering(cmdBuf);

        colorImageToShaderRead(cmdBuf, mip.image);

        size = mip.size;
        sampleIndex = constants::BloomFirstMip + i;
      }
    }

    {
      KT_VK_ZONE(m.tracyContext, cmdBuf, "Bloom Upsample Passes");
      for (size_t i = constants::BLOOM_MIP_LEVELS - 1; i > 0; i--) {
        size_t downsampledIndex = i;
        size_t upsampledIndex = i - 1;
        auto& mip = m.renderTargets.bloomMips[upsampledIndex];
        colorImageToRenderable(cmdBuf, mip.image);
        colorImageBeginRendering(cmdBuf, mip.image, false);

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.bloomUpsample.pipeline);
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.bloomUpsample.layout, 0, 1,
                                &m.globalDescriptorSets.sets[m.frameInfo.index], 0, nullptr);

        setupCustomViewportAndScissor(cmdBuf, {}, {mip.size.x, mip.size.y});

        struct PushConstants {
          float filterRadius;
          uint32_t level;
        } pushConstants{
            .filterRadius = filterRadius,
            .level = static_cast<uint32_t>(constants::BloomFirstMip + downsampledIndex),
        };
        vkCmdPushConstants(cmdBuf, m.pipelines.bloomUpsample.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(PushConstants), &pushConstants);

        vkCmdDraw(cmdBuf, 3, 1, 0, 0);

        vkCmdEndRendering(cmdBuf);

        colorImageToShaderRead(cmdBuf, mip.image);
      }
    }
    {
      KT_VK_ZONE(m.tracyContext, cmdBuf, "Bloom Combine Pass");
      {
        VkImageMemoryBarrier2 barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .srcQueueFamilyIndex = m.vkcore.queues.present.index,
            .dstQueueFamilyIndex = m.vkcore.queues.graphics.index,
            .image = m.vkcore.swapchain.nImage(m.frameInfo.imageIndex),
            .subresourceRange =
                VkImageSubresourceRange{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };
        VkDependencyInfo dependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier,
        };
        vkCmdPipelineBarrier2(cmdBuf, &dependencyInfo);
      }
      {
        VkRenderingAttachmentInfo aInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = m.vkcore.swapchain.nView(m.frameInfo.imageIndex),
            .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };
        VkRenderingInfo renderingInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea =
                VkRect2D{
                    .offset = VkOffset2D{.x = 0, .y = 0},
                    .extent = m.vkcore.swapchain.config().extent,
                },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &aInfo,
        };
        vkCmdBeginRendering(cmdBuf, &renderingInfo);
      }

      vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.bloomCombine.pipeline);
      vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.bloomCombine.layout, 0, 1,
                              &m.globalDescriptorSets.sets[m.frameInfo.index], 0, nullptr);

      auto& extent = m.vkcore.swapchain.config().extent;
      setupCustomViewportAndScissor(cmdBuf, {}, {extent.width, extent.height});

      vkCmdDraw(cmdBuf, 3, 1, 0, 0);
      vkCmdEndRendering(cmdBuf);
    }
  }

  void Renderer::newFrame() {
    KT_PROFILE_FUNCTION
    imGuiNewFrame();
    auto& perFrame = m.vkcore.perFrame[m.frameInfo.index];

    auto nextImageRes = m.vkcore.swapchain.getNextImage(m.vkcore.device, perFrame.inFlightFence, perFrame.imageAvailableSemaphore);

    if (!nextImageRes) {
      VK_CRITICAL("Failed to acquire next swapchain image: {}", nextImageRes.error());
      abort();
    }
    auto [imageIndex, swapchainState] = nextImageRes.value();

    if (swapchainState == vkh::Swapchain::State::OutOfDate) {
      auto res = recreateSwapchain();
      if (!res) {
        VK_CRITICAL("Failed to recreate swapchain: {}", res.error());
        abort();
      }
      VK_DEBUG("Restarting frame after swapchain recreation");
      // Try again
      newFrame();
    }

    m.frameInfo.imageIndex = imageIndex;
    m.frameInfo.perFrame = &perFrame;

    if (swapchainState == vkh::Swapchain::State::Suboptimal) {
      m.frameInfo.suboptimalSwapchain = true;
    }
  }

  void Renderer::startFrame() {
    KT_PROFILE_FUNCTION
    VK_ASSERT(m.frameInfo.perFrame->pools.graphics.pool != VK_NULL_HANDLE, "Graphics command pool is null");
    VK_ASSERT(m.frameInfo.perFrame->pools.compute.pool != VK_NULL_HANDLE, "Compute command pool is null");
    m.frameInfo.perFrame->pools.resetAll(m.vkcore.device.logical);

    updateTextureDescriptors();
  }

  void Renderer::renderImGui(VkCommandBuffer cmdBuf) {
    KT_PROFILE_FUNCTION
    KT_VK_ZONE(m.tracyContext, cmdBuf, "Render ImGui");
    ImGui::Render();

    VkRenderingAttachmentInfo aInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = m.vkcore.swapchain.nView(m.frameInfo.imageIndex),
        .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };

    VkRenderingInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea =
            VkRect2D{
                .offset = VkOffset2D{.x = 0, .y = 0},
                .extent = m.vkcore.swapchain.config().extent,
            },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &aInfo,
    };

    vkCmdBeginRendering(cmdBuf, &renderingInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);
    vkCmdEndRendering(cmdBuf);
  }

  void Renderer::endFrame(VkCommandBuffer cmdBuf) {
    KT_PROFILE_FUNCTION
    auto& sem = m.vkcore.swapchain.nPresentSemaphore(m.frameInfo.imageIndex);

    std::array waitInfo{
        VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = m.vkcore.perFrame[m.frameInfo.index].imageAvailableSemaphore,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        },
        VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = m.vkcore.perFrame[m.frameInfo.index].lightsFinished,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        },
    };

    VkSemaphoreSubmitInfo signalInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = sem,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .deviceIndex = 0,
    };

    VkCommandBufferSubmitInfo cmdBufInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmdBuf,
        .deviceMask = 0,
    };

    VkSubmitInfo2 submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = waitInfo.size(),
        .pWaitSemaphoreInfos = waitInfo.data(),
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdBufInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalInfo,
    };

    vkResetFences(m.vkcore.device.logical, 1, &m.vkcore.perFrame[m.frameInfo.index].inFlightFence);

    auto res = vkQueueSubmit2(m.vkcore.queues.graphics.queue, 1, &submitInfo, m.vkcore.perFrame[m.frameInfo.index].inFlightFence);
    VK_ASSERT(res == VK_SUCCESS, "Failed to submit command buffer: {}", res);

    present();
  }

  void Renderer::present() {
    KT_PROFILE_FUNCTION
    uint32_t imageIndex = m.frameInfo.imageIndex;
    auto& sem = m.vkcore.swapchain.nPresentSemaphore(imageIndex);

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &sem,
        .swapchainCount = 1,
        .pSwapchains = &m.vkcore.swapchain.getSwapchain(),
        .pImageIndices = &imageIndex,
    };

    auto result = vkQueuePresentKHR(m.vkcore.queues.present.queue, &presentInfo);

    m.frameInfo.index = m.frameInfo.nextIndex; // Advance to next frame index

    m.frameInfo.nextIndex = (m.frameInfo.nextIndex + 1) % MAX_FRAMES_IN_FLIGHT;

#ifndef NDEBUG
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      VK_DEBUG("Swapchain is out of date during present");
    } else if (result == VK_SUBOPTIMAL_KHR) {
      VK_DEBUG("Swapchain is suboptimal during present");
    } else if (m.frameInfo.suboptimalSwapchain) {
      VK_DEBUG("Swapchain was suboptimal at image aquire");
    }
#endif

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m.frameInfo.suboptimalSwapchain) {
      auto res = recreateSwapchain();
      if (!res) {
        VK_CRITICAL("Failed to recreate swapchain: {}", res.error());
        abort();
      }
    }

    KT_MARK_FRAME;
  }

  std::expected<void, std::string> Renderer::recreateSwapchain() {
    KT_PROFILE_FUNCTION
    VK_TRACE("Recreating swapchain");
    VKH_MAKE(newSwapchain,
             setup::createSwapchain(m.vkcore.device.physical, m.window->getRenderSize(), m.vkcore.device.logical, m.vkcore.surface,
                                    m.vkcore.queues, m.vkcore.swapchain.getSwapchain()),
             "Failed to recreate swapchain");

    {
      [[maybe_unused]]
      auto oldSwapchain = std::move(m.vkcore.swapchain);

      m.vkcore.swapchain = std::move(newSwapchain);

      VK_DEBUG("Waiting for device idle after swapchain recreation");
      vkDeviceWaitIdle(m.vkcore.device.logical);
    }

    m.frameInfo.suboptimalSwapchain = false;

    VK_DEBUG("Swapchain recreated.");
    return {};
  }

} // namespace kt::vkh
