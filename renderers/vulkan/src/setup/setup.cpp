#include "setup.hpp"

#include "keptech/vulkan/helpers/descriptors.hpp"
#include "keptech/vulkan/renderer.hpp"

#include "keptech/core/window.hpp"
#include "macros.hpp"
#include <SDL3/SDL_vulkan.h>
#include <expected>
#include <keptech/components/camera.hpp>
#include <keptech/maths/maths.hpp>

namespace kt::vkh {
  using namespace kt::vkh::setup;

  std::expected<Renderer, std::string> Renderer::create(const RendererCreateInfo& createInfo, const core::window::Window& window) {
    VKH_MAKE(vkcore, createVulkanCore(createInfo, window), "Failed to create Vulkan core.");

    size_t size = sizeof(components::Camera::Uniforms);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      size += sizeof(components::Camera::Uniforms);
      size = maths::roundToAlignment(size, 256);
    }

    VkBufferCreateInfo bufInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_MAPPED_BIT |
                 VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
        .usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    VKH_MAKE(globalDescriptorSets, createGlobalDescriptors(vkcore.device.logical), "Failed to create global descriptor sets.");

    VKH_MAKE(buffers, createBuffers(vkcore), "Failed to create buffers for renderer.");
    VKH_MAKE(pipelines, createPipelines(vkcore), "Failed to create pipelines for renderer.");

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(vkcore.device.physical, &properties);

    Limits limits{
        .minUniformBufferOffsetAlignment = properties.limits.minUniformBufferOffsetAlignment,
    };

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
      offset = maths::roundToAlignment(offset, limits.minUniformBufferOffsetAlignment);
    }

    VK_DEBUG("Vulkan renderer created successfully.");

    Renderer r{{
        .window = &window,
        .vkcore = std::move(vkcore),
        .limits = limits,
        .buffers = buffers,
        .pipelines = pipelines,
        .globalDescriptorSets = globalDescriptorSets,
    }};

    return std::move(r);
  }
} // namespace kt::vkh
