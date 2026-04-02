#include "setup.hpp"

#include "keptech/core/window.hpp"
#include "keptech/vulkan/constants.hpp"
#include "keptech/vulkan/helpers/descriptors.hpp"
#include "keptech/vulkan/renderer.hpp"
#include "macros.hpp"
#include <SDL3/SDL_vulkan.h>
#include <expected>
#include <keptech/components/camera.hpp>
#include <keptech/maths/maths.hpp>

namespace kt::vkh {
  using namespace kt::vkh::setup;

  std::expected<Renderer, std::string> Renderer::create(const RendererCreateInfo& createInfo, const core::window::Window& window) {
    VKH_MAKE(vkcore, createVulkanCore(createInfo, window), "Failed to create Vulkan core.");
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(vkcore.device.physical, &properties);

    limits::minUniformBufferOffsetAlignment = properties.limits.minUniformBufferOffsetAlignment;

    VKH_MAKE(globalDescriptorSets, createGlobalDescriptors(vkcore.device.logical), "Failed to create global descriptor sets.");

    VKH_MAKE(buffers, createBuffers(vkcore), "Failed to create buffers for renderer.");

    size_t offset = 0;
    for (auto& set : globalDescriptorSets.sets) {
      DescriptorWriter writer{};
      writer.writeBuffer(0,
                         VkDescriptorBufferInfo{
                             .buffer = buffers.camera.buffer,
                             .offset = offset,
                             .range = sizeof(components::Camera::Uniforms),
                         },
                         DescriptorWriter::BufferType::Uniform);

      writer.update(vkcore.device.logical, set);

      offset += sizeof(components::Camera::Uniforms);
      offset = maths::roundToAlignment(offset, limits::minUniformBufferOffsetAlignment);
    }

    VK_DEBUG("Vulkan renderer created successfully.");

    VKH_MAKE(pipelines, createPipelines(vkcore, globalDescriptorSets.layout), "Failed to create pipelines for renderer.");

    Renderer r{{
        .window = &window,
        .vkcore = std::move(vkcore),
        .buffers = buffers,
        .pipelines = pipelines,
        .globalDescriptorSets = globalDescriptorSets,
    }};

    return std::move(r);
  }
} // namespace kt::vkh
