#include "setup.hpp"

#ifdef KT_PROFILE
#include "profile.hpp"
#endif
#include "rhi.hpp"
#include <SDL3/SDL_vulkan.h>

#include "constants.hpp"
#include "graph/builder.hpp"
#include "graph/graph.hpp"
#include <keptech/components/camera.hpp>
#include <keptech/maths/maths.hpp>

namespace kt::rhi {
  using namespace kt::rhi::setup;

  std::expected<void, std::string> RHI::init(const RendererCreateInfo& createInfo, const Window& window) {
    if (isInitialized) {
      return std::unexpected("Renderer is already initialized.");
    }

    return singleton.initInternal(createInfo, window);
  }

  std::expected<void, std::string> RHI::initInternal(const RendererCreateInfo& createInfo, const Window& window) {
    m.window = &window;

    {
      auto res = initVulkanCore(createInfo, window);
      if (!res) {
        return std::unexpected(res.error());
      }
    }

    VkPhysicalDeviceMaintenance3Properties maintenance3Properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES,
    };
    VkPhysicalDeviceProperties2 properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &maintenance3Properties,
    };
    vkGetPhysicalDeviceProperties2(m.vkcore.device, &properties);

    limits::minUniformBufferOffsetAlignment = properties.properties.limits.minUniformBufferOffsetAlignment;
    limits::minStorageBufferOffsetAlignment = properties.properties.limits.minStorageBufferOffsetAlignment;
    limits::maxPushConstantsSize = properties.properties.limits.maxPushConstantsSize;

    if (properties.pNext) {
      VkPhysicalDeviceMaintenance3Properties p = *reinterpret_cast<VkPhysicalDeviceMaintenance3Properties*>(properties.pNext);
      limits::maxMemoryAllocationSize = p.maxMemoryAllocationSize;
    }

    auto samplers_res = initSamplers();
    if (!samplers_res) {
      return std::unexpected(samplers_res.error());
    }

    auto imGui_res = initImGui();
    if (!imGui_res) {
      return std::unexpected(imGui_res.error());
    }

    auto desc_res = initDescriptors();
    if (!desc_res) {
      return std::unexpected(desc_res.error());
    }

    auto buffers_res = initBuffers();
    if (!buffers_res) {
      return std::unexpected(buffers_res.error());
    }

    auto formats_res = initFormats();
    if (!formats_res) {
      return std::unexpected(formats_res.error());
    }

#ifdef KT_PROFILE
    m.tracyGraphicsContext = KT_VK_CONTEXT(vkcore.device, vkcore.device);
    m.tracyComputeContext = KT_VK_CONTEXT(vkcore.device, vkcore.device);
    KT_VK_CONTEXT_NAME(m.tracyGraphicsContext, "Graphics");
    KT_VK_CONTEXT_NAME(m.tracyComputeContext, "Compute");
#endif

    vkDeviceWaitIdle(m.vkcore.device);

    writeDescriptors();

    return {};
  }
} // namespace kt::rhi
