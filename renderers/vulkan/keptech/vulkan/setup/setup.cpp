#include "setup.hpp"

#include "keptech/core/window.hpp"
#include "keptech/vulkan/constants.hpp"
#include "keptech/vulkan/renderer.hpp"
#include "macros.hpp"
#include "profile.hpp"
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

    VKH_MAKE(samplers, createSamplers(vkcore.device.logical), "Failed to create samplers for renderer.");

    VKH_MAKE(imGuiObjects, setupImGui(window, vkcore, samplers), "Failed to set up ImGui for Vulkan.");

    VKH_MAKE(globalDescriptorSets, createGlobalDescriptors(vkcore.device.logical), "Failed to create global descriptor sets.");
    VKH_MAKE(staticDescriptorSets, createStaticDescriptors(vkcore), "Failed to create static descriptor sets.");

    VKH_MAKE(buffers, createBuffers(vkcore), "Failed to create buffers for renderer.");
    VKH_MAKE(formats, findFormats(vkcore), "Failed to find suitable formats for renderer.");

    VKH_MAKE(pipelines, createPipelines(vkcore, formats, globalDescriptorSets.layout, staticDescriptorSets.layout),
             "Failed to create pipelines for renderer.");

    auto d = SDL_GetDisplayForWindow(window.getHandle());
    auto* dm = SDL_GetCurrentDisplayMode(d);

    VKH_MAKE(renderTargets, createRenderTargets(vkcore, formats, glm::ivec2{dm->w, dm->h}),
             "Failed to create render targets for renderer.");

    {
      auto res = writeSsao(vkcore, buffers, renderTargets);
      if (!res) {
        VK_CRITICAL("Failed to write SSAO noise texture: {}", res.error());
        abort();
      }
    }

    writeGlobalDescriptors(vkcore, globalDescriptorSets, buffers);
    writeStaticDescriptors(vkcore, staticDescriptorSets, buffers, renderTargets, samplers);

#ifdef KT_PROFILE
    auto gctx = KT_VK_CONTEXT(vkcore.device.physical, vkcore.device.logical);
    auto cctx = KT_VK_CONTEXT(vkcore.device.physical, vkcore.device.logical);
    KT_VK_CONTEXT_NAME(gctx, "Graphics");
    KT_VK_CONTEXT_NAME(cctx, "Compute");
#endif

    vkDeviceWaitIdle(vkcore.device.logical);

    VK_DEBUG("Vulkan renderer created successfully.");
    Renderer r{Renderer::Members{
        .window = &window,
        .vkcore = std::move(vkcore),
        .samplers = samplers,
        .imGuiDescriptorPool = imGuiObjects,
        .formats = formats,
        .renderTargets = std::move(renderTargets),
        .buffers = std::move(buffers),
        .pipelines = pipelines,
        .globalDescriptorSets = globalDescriptorSets,
        .staticDescriptors = staticDescriptorSets,
#ifdef KT_PROFILE
        .tracyGraphicsContext = gctx,
        .tracyComputeContext = cctx,
#endif
    }};

    return std::move(r);
  }
} // namespace kt::vkh
