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

  void GBuffer::destroy(const VmaAllocator& allocator, const VkDevice& device) {
    albedo.destroy(allocator, device);
    normal.destroy(allocator, device);
    emissive.destroy(allocator, device);
    metRough.destroy(allocator, device);
    depth.destroy(allocator, device);
  }

  void LightBuffer::destroy(const VmaAllocator& allocator, const VkDevice& device) {
    diffuse.destroy(allocator, device);
    specular.destroy(allocator, device);
    combined.destroy(allocator, device);
  }

  void Renderer::RenderTargets::destroy(const VmaAllocator& allocator, const VkDevice& device) {
    gBuffer.destroy(allocator, device);
    lights.destroy(allocator, device);
  }

  void Renderer::Pipelines::destroy(const VkDevice& device) {
    vkDestroyPipeline(device, basic.pipeline, nullptr);
    vkDestroyPipelineLayout(device, basic.layout, nullptr);

    vkDestroyPipeline(device, deferred.pipeline, nullptr);
    vkDestroyPipelineLayout(device, deferred.layout, nullptr);

    vkDestroyPipeline(device, deferredPointLight.pipeline, nullptr);
    vkDestroyPipelineLayout(device, deferredPointLight.layout, nullptr);

    vkDestroyPipeline(device, deferredCombine.pipeline, nullptr);
    vkDestroyPipelineLayout(device, deferredCombine.layout, nullptr);
  }

  void Renderer::Buffers::destroy(VmaAllocator& allocator) { camera.destroy(allocator); }

  Renderer::~Renderer() {
    if (m.moveGuard.moved()) {
      return;
    }

    auto& device = m.vkcore.device.logical;
    auto& allocator = m.vkcore.allocator;

    vkDeviceWaitIdle(device);

    for (auto& perFrame : m.vkcore.perFrame) {
      vkDestroySemaphore(device, perFrame.imageAvailableSemaphore, nullptr);
      vkDestroyFence(device, perFrame.inFlightFence, nullptr);

      vkDestroyCommandPool(device, perFrame.pools.graphics.pool, nullptr);
      vkDestroyCommandPool(device, perFrame.pools.compute.pool, nullptr);
    }
    vkDestroyCommandPool(device, m.vkcore.transferPool.pool, nullptr);

    vkDestroyDescriptorSetLayout(device, m.globalDescriptorSets.layout, nullptr);
    vkDestroyDescriptorPool(device, m.globalDescriptorSets.pool, nullptr);

    m.buffers.destroy(allocator);

    for (auto& texture : m.loadedTextures) {
      texture.destroy(allocator, device);
    }
    for (auto& buffer : m.loadedBuffers) {
      buffer.destroy(allocator);
    }

    m.pipelines.destroy(device);

    m.renderTargets.destroy(allocator, device);

    vmaDestroyAllocator(allocator);

    shutdownImGui();
    m.vkcore.swapchain.~Swapchain();
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(m.vkcore.instance, m.vkcore.surface, nullptr);
    vkDestroyInstance(m.vkcore.instance, nullptr);

    VK_INFO("Vulkan renderer shut down cleanly");
  }

} // namespace kt::vkh
