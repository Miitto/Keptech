#include "keptech/vulkan/renderer.hpp"

#include "keptech/rendering/imgui.hpp"
#include "profile.hpp"
#include "vk-logger.hpp"

namespace kt::vkh {
  void Renderer::imGuiNewFrame() const {
    ImGui_ImplVulkan_NewFrame();
    rendering::newImGuiFrame();
  }

  void Renderer::shutdownImGui() {
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(m.vkcore.device, m.imGuiDescriptorPool, nullptr);
    VK_DEBUG("Shut down ImGui Vulkan backend");
    rendering::shutdownImGui();
  }

  void Samplers::destroy(const VkDevice device) {
    vkDestroySampler(device, linearRepeat, nullptr);
    vkDestroySampler(device, linearClamp, nullptr);
    vkDestroySampler(device, nearestRepeat, nullptr);
    vkDestroySampler(device, nearestClamp, nullptr);
  }

  Renderer::~Renderer() {
    auto& device = m.vkcore.device;
    auto& allocator = m.vkcore.allocator;

    vkDeviceWaitIdle(device);

    KT_VK_CONTEXT_DESTROY(m.tracyGraphicsContext);
    KT_VK_CONTEXT_DESTROY(m.tracyComputeContext);

    for (auto& perFrame : m.vkcore.perFrame) {
      vkDestroySemaphore(device, perFrame.imageAvailableSemaphore, nullptr);
      vkDestroyFence(device, perFrame.inFlightFence, nullptr);

      vkDestroyCommandPool(device, perFrame.pools.graphics.pool, nullptr);
      vkDestroyCommandPool(device, perFrame.pools.compute.pool, nullptr);
    }
    vkDestroyCommandPool(device, m.vkcore.transferPool.pool, nullptr);

    vkDestroyDescriptorSetLayout(device, m.globalDescriptorSets.layout, nullptr);
    vkDestroyDescriptorPool(device, m.globalDescriptorSets.pool, nullptr);

    m.buffers.~Buffers();

    for (auto& texture : m.loadedTextures) {
      vkDestroyImageView(device, texture.view, nullptr);
      vmaDestroyImage(allocator, texture.image, texture.alloc);
    }
    for (auto& buffer : m.loadedBuffers) {
      buffer.destroy();
    }

    m.samplers.destroy(device);

    vkDestroySemaphore(device, m.vkcore.mainSemaphore.semaphore, nullptr);
    vkDestroySemaphore(device, m.vkcore.transferSemaphore.semaphore, nullptr);

    vmaDestroyAllocator(allocator);

    shutdownImGui();
    m.vkcore.swapchain.destroy();

    vkDeviceWaitIdle(device); // Sometimes complains that some of the swapchain resources havn't been destroyed yet.
    device.destroy();

    vkDestroySurfaceKHR(m.vkcore.instance, m.vkcore.surface, nullptr);
    m.vkcore.instance.destroy();

    VK_INFO("Vulkan renderer shut down cleanly");
  }

} // namespace kt::vkh
