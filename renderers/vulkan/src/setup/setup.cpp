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

    VkPhysicalDeviceMaintenance3Properties maintenance3Properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES,
    };
    VkPhysicalDeviceProperties2 properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &maintenance3Properties,
    };
    vkGetPhysicalDeviceProperties2(vkcore.device.physical, &properties);

    limits::minUniformBufferOffsetAlignment = properties.properties.limits.minUniformBufferOffsetAlignment;
    limits::minStorageBufferOffsetAlignment = properties.properties.limits.minStorageBufferOffsetAlignment;
    limits::maxPushConstantsSize = properties.properties.limits.maxPushConstantsSize;

    if (properties.pNext) {
      VkPhysicalDeviceMaintenance3Properties p = *reinterpret_cast<VkPhysicalDeviceMaintenance3Properties*>(properties.pNext);
      limits::maxMemoryAllocationSize = p.maxMemoryAllocationSize;
    }

    VKH_MAKE(imGuiObjects, setupImGui(window, vkcore), "Failed to set up ImGui for Vulkan.");

    VKH_MAKE(globalDescriptorSets, createGlobalDescriptors(vkcore.device.logical), "Failed to create global descriptor sets.");

    VKH_MAKE(buffers, createBuffers(vkcore), "Failed to create buffers for renderer.");
    VKH_MAKE(formats, findFormats(vkcore), "Failed to find suitable formats for renderer.");

    VKH_MAKE(pipelines, createPipelines(vkcore, formats, globalDescriptorSets.layout), "Failed to create pipelines for renderer.");

    auto d = SDL_GetDisplayForWindow(window.getHandle());
    auto* dm = SDL_GetCurrentDisplayMode(d);

    VKH_MAKE(renderTargets, createRenderTargets(vkcore, formats, glm::ivec2{dm->w, dm->h}),
             "Failed to create render targets for renderer.");

    size_t offset = 0;
    std::array imageInfos = {
        VkDescriptorImageInfo{
            .sampler = imGuiObjects.sampler,
            .imageView = renderTargets.gBuffer.albedo.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = imGuiObjects.sampler,
            .imageView = renderTargets.gBuffer.normal.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = imGuiObjects.sampler,
            .imageView = renderTargets.gBuffer.emissive.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = imGuiObjects.sampler,
            .imageView = renderTargets.gBuffer.metRough.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = imGuiObjects.sampler,
            .imageView = renderTargets.gBuffer.depth.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = imGuiObjects.sampler,
            .imageView = renderTargets.lights.diffuse.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = imGuiObjects.sampler,
            .imageView = renderTargets.lights.specular.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = imGuiObjects.sampler,
            .imageView = renderTargets.lights.combined.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
    };
    VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 8,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = imageInfos.data(),
    };

    for (auto& set : globalDescriptorSets.sets) {
      write.dstSet = set;
      vkUpdateDescriptorSets(vkcore.device.logical, 1, &write, 0, nullptr);
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
    Renderer r{{
        .window = &window,
        .vkcore = std::move(vkcore),
        .imGuiObjects = imGuiObjects,
        .formats = formats,
        .renderTargets = renderTargets,
        .buffers = buffers,
        .pipelines = pipelines,
        .globalDescriptorSets = globalDescriptorSets,
    }};

    return std::move(r);
  }
} // namespace kt::vkh
