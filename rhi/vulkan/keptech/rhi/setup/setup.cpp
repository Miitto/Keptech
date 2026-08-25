#include "setup.hpp"

#ifdef KT_PROFILE
#include "profile.hpp"
#endif

#include "rhi.hpp"

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

    constants::minUniformBufferOffsetAlignment = properties.properties.limits.minUniformBufferOffsetAlignment;
    constants::minStorageBufferOffsetAlignment = properties.properties.limits.minStorageBufferOffsetAlignment;
    constants::maxPushConstantsSize = properties.properties.limits.maxPushConstantsSize;

    if (properties.pNext) {
      VkPhysicalDeviceMaintenance3Properties p = *reinterpret_cast<VkPhysicalDeviceMaintenance3Properties*>(properties.pNext);
      constants::maxMemoryAllocationSize = p.maxMemoryAllocationSize;
    }

    auto imGui_res = initImGui();
    if (!imGui_res) {
      return std::unexpected(imGui_res.error());
    }

#ifdef KT_PROFILE
    m.tracyGraphicsContext = KT_VK_CONTEXT(vkcore.device, vkcore.device);
    m.tracyComputeContext = KT_VK_CONTEXT(vkcore.device, vkcore.device);
    KT_VK_CONTEXT_NAME(m.tracyGraphicsContext, "Graphics");
    KT_VK_CONTEXT_NAME(m.tracyComputeContext, "Compute");
#endif

    vkDeviceWaitIdle(m.vkcore.device);

    return {};
  }
} // namespace kt::rhi
