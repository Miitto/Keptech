#include "setup.hpp"

#include "macros.hpp"
#include "profile.hpp"
#include "renderer.hpp"
#include <SDL3/SDL_vulkan.h>

#include "constants.hpp"
#include "renderGraph/builder.hpp"
#include "renderGraph/graph.hpp"
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
    vkGetPhysicalDeviceProperties2(vkcore.device, &properties);

    limits::minUniformBufferOffsetAlignment = properties.properties.limits.minUniformBufferOffsetAlignment;
    limits::minStorageBufferOffsetAlignment = properties.properties.limits.minStorageBufferOffsetAlignment;
    limits::maxPushConstantsSize = properties.properties.limits.maxPushConstantsSize;

    if (properties.pNext) {
      VkPhysicalDeviceMaintenance3Properties p = *reinterpret_cast<VkPhysicalDeviceMaintenance3Properties*>(properties.pNext);
      limits::maxMemoryAllocationSize = p.maxMemoryAllocationSize;
    }

    VKH_MAKE(samplers, createSamplers(vkcore.device), "Failed to create samplers for renderer.");

    VKH_MAKE(imGuiObjects, setupImGui(window, vkcore, samplers), "Failed to set up ImGui for Vulkan.");

    VKH_MAKE(globalDescriptorSets, createGlobalDescriptors(vkcore.device), "Failed to create global descriptor sets.");
    VKH_MAKE(staticDescriptorSets, createStaticDescriptors(vkcore), "Failed to create static descriptor sets.");

    VKH_MAKE(buffers, createBuffers(vkcore), "Failed to create buffers for renderer.");
    VKH_MAKE(formats, findFormats(vkcore), "Failed to find suitable formats for renderer.");

#ifdef KT_PROFILE
    auto gctx = KT_VK_CONTEXT(vkcore.device, vkcore.device);
    auto cctx = KT_VK_CONTEXT(vkcore.device, vkcore.device);
    KT_VK_CONTEXT_NAME(gctx, "Graphics");
    KT_VK_CONTEXT_NAME(cctx, "Compute");
#endif

    vkDeviceWaitIdle(vkcore.device);

    VK_DEBUG("Creating test render graph.");

    Renderer::Members members{
        .window = &window,
        .vkcore = std::move(vkcore),
        .samplers = samplers,
        .imGuiDescriptorPool = imGuiObjects,
        .formats = formats,
        .buffers = std::move(buffers),
        .globalDescriptorSets = globalDescriptorSets,
        .staticDescriptors = staticDescriptorSets,
#ifdef KT_PROFILE
        .tracyGraphicsContext = gctx,
        .tracyComputeContext = cctx,
#endif
    };

    VK_DEBUG("Writing Global Descriptor Sets.");
    writeGlobalDescriptors(members, members.globalDescriptorSets);
    VK_DEBUG("Writing Static Descriptor Sets.");
    writeStaticDescriptors(members.vkcore, members.staticDescriptors, members.buffers);

    Renderer r{std::move(members)};

    return std::move(r);
  }
} // namespace kt::vkh
