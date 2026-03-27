#include "keptech/vulkan/helpers/descriptors.hpp"
#include "keptech/vulkan/renderer.hpp"
#include "setup/core.hpp"

#include "descriptors.hpp"
#include "keptech/core/window.hpp"
#include "keptech/vulkan/helpers/device.hpp"
#include "macros.hpp"
#include <SDL3/SDL_vulkan.h>
#include <expected>
#include <keptech/core/components/camera.hpp>
#include <keptech/core/maths/maths.hpp>

namespace kt::vkh {
  using namespace kt::vkh::setup;

  std::expected<RendererBackend, std::string>
  RendererBackend::create(const RendererCreateInfo& createInfo,
                          const core::window::Window& window) {
    VKH_MAKE(vkcore, createVulkanCore(createInfo, window),
             "Failed to create Vulkan core.");

    size_t size = sizeof(components::Camera::Uniforms);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      size += sizeof(components::Camera::Uniforms);
      size = maths::roundToAlignment(size, 256);
    }

    vk::BufferCreateInfo bufInfo{
        .size = size,
        .usage = vk::BufferUsageFlagBits::eTransferDst |
                 vk::BufferUsageFlagBits::eUniformBuffer,
    };

    vma::AllocationCreateInfo allocInfo{
        .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
                 vma::AllocationCreateFlagBits::eMapped |
                 vma::AllocationCreateFlagBits::eHostAccessAllowTransferInstead,
        .usage = vma::MemoryUsage::eAutoPreferDevice,
    };

    VKH_MAKE(cameraBuffer,
             AllocatedBuffer::create(vkcore.allocator, bufInfo, allocInfo,
                                     "Camera Uniform Buffer"),
             "Failed to create camera uniform buffer.");

    VKH_MAKE(globalDescriptorSets,
             createGlobalDescriptors(vkcore.device.logical),
             "Failed to create global descriptor sets.");
    Limits limits{
        .minUniformBufferOffsetAlignment =
            vkcore.device.physical.getProperties()
                .limits.minUniformBufferOffsetAlignment,
    };

    size_t offset = 0;
    for (auto& set : globalDescriptorSets.sets) {
      DescriptorWriter writer{};
      writer.writeBuffer(0,
                         vk::DescriptorBufferInfo{
                             .buffer = cameraBuffer.buffer,
                             .offset = offset,
                             .range = sizeof(components::Camera::Uniforms),
                         },
                         DescriptorWriter::BufferType::Uniform);

      writer.update(vkcore.device.logical, *set);

      offset += sizeof(components::Camera::Uniforms);
      offset = maths::roundToAlignment(offset,
                                       limits.minUniformBufferOffsetAlignment);
    }

    VK_DEBUG("Vulkan renderer created successfully.");

    RendererBackend r{{
        .window = &window,
        .vkcore = std::move(vkcore),
        .limits = limits,
        .cameraBuffer = cameraBuffer,
        .globalDescriptorSets = std::move(globalDescriptorSets),
    }};

    return std::move(r);
  }
} // namespace kt::vkh
