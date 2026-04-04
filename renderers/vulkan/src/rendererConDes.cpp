#include "keptech/vulkan/renderer.hpp"

#include "keptech/rendering/imgui.hpp"
#include "vk-logger.hpp"

namespace kt::vkh {
  Renderer::Renderer(Renderer&& o) noexcept : m(std::move(o.m)) { m.frameInfo.perFrame = &m.vkcore.perFrame[m.frameInfo.index]; }

  Renderer& Renderer::operator=(Renderer&& o) noexcept {
    if (this == &o)
      return *this;

    m = std::move(o.m);
    return *this;
  }

  void Renderer::imGuiNewFrame() const {
    ImGui_ImplVulkan_NewFrame();
    rendering::newImGuiFrame();
  }

  void Renderer::shutdownImGui() {
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(m.vkcore.device.logical, m.imGuiObjects.descriptorPool, nullptr);
    vkDestroySampler(m.vkcore.device.logical, m.imGuiObjects.sampler, nullptr);
    VK_DEBUG("Shut down ImGui Vulkan backend");
    rendering::shutdownImGui();
  }

  Renderer::~Renderer() {
    if (m.moveGuard.moved()) {
      return;
    }

    auto& device = m.vkcore.device.logical;
    auto& allocator = m.vkcore.allocator;

    vkDeviceWaitIdle(device);

    auto destroyPipeline = [&](Pipeline& pipeline) {
      vkDestroyPipeline(device, pipeline.pipeline, nullptr);
      vkDestroyPipelineLayout(device, pipeline.layout, nullptr);
    };

    for (auto& perFrame : m.vkcore.perFrame) {
      vkDestroySemaphore(device, perFrame.imageAvailableSemaphore, nullptr);
      vkDestroyFence(device, perFrame.inFlightFence, nullptr);

      vkDestroyCommandPool(device, perFrame.pools.graphics.pool, nullptr);
      vkDestroyCommandPool(device, perFrame.pools.compute.pool, nullptr);
    }
    vkDestroyCommandPool(device, m.vkcore.transferPool.pool, nullptr);

    vkDestroyDescriptorSetLayout(device, m.globalDescriptorSets.layout, nullptr);
    vkDestroyDescriptorPool(device, m.globalDescriptorSets.pool, nullptr);

    m.buffers.camera.destroy(allocator);

    for (auto& texture : m.loadedTextures) {
      texture.destroy(allocator, device);
    }
    for (auto& buffer : m.loadedBuffers) {
      buffer.destroy(allocator);
    }

    destroyPipeline(m.pipelines.deferred);

    vmaDestroyAllocator(allocator);

    shutdownImGui();
    m.vkcore.swapchain.~Swapchain();
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(m.vkcore.instance, m.vkcore.surface, nullptr);
    vkDestroyInstance(m.vkcore.instance, nullptr);

    VK_INFO("Vulkan renderer shut down cleanly");
  }

} // namespace kt::vkh
