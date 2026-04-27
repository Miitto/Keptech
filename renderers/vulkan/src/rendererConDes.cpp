#include "keptech/vulkan/renderer.hpp"

#include "keptech/rendering/imgui.hpp"
#include "profile.hpp"
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
    vkDestroyDescriptorPool(m.vkcore.device.logical, m.imGuiDescriptorPool, nullptr);
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
    ssaoResult.destroy(allocator, device);
    ssaoNoise.destroy(allocator, device);
    ssaoBlur.destroy(allocator, device);
    combined.destroy(allocator, device);
  }

  void Renderer::RenderTargets::destroy(const VmaAllocator& allocator, const VkDevice& device) {
    gBuffer.destroy(allocator, device);
    lights.destroy(allocator, device);

    for (auto& bloomMip : bloomMips) {
      bloomMip.image.destroy(allocator, device);
    }
  }

  void Renderer::Pipelines::destroy(const VkDevice& device) {
    auto d = [&](Pipeline& pipeline) {
      vkDestroyPipeline(device, pipeline.pipeline, nullptr);
      vkDestroyPipelineLayout(device, pipeline.layout, nullptr);
    };
    d(basic);
    d(deferred);
    d(pointLightShadows);
    d(deferredPointLight);
    d(ssao);
    d(ssaoBlur);
    d(deferredCombine);
    d(bloomDownsample);
    d(bloomUpsample);
    d(bloomCombine);
  }

  void Renderer::PerFrameBuffers::destroy(VmaAllocator& allocator) {
    objects.buffer.destroy(allocator);
    pointLights.buffer.destroy(allocator);
    shadowMatrices.buffer.destroy(allocator);
    drawCommands.buffer.destroy(allocator);
    shadowDrawCommands.buffer.destroy(allocator);
  }

  void Renderer::Buffers::destroy(VmaAllocator& allocator) {
    camera.destroy(allocator);
    ssaoKernel.destroy(allocator);
    vertices.buffer.destroy(allocator);
    indices.buffer.destroy(allocator);
    materials.buffer.destroy(allocator);

    for (auto& perFrame : perFrame) {
      perFrame.destroy(allocator);
    }
  }

  void Renderer::Samplers::destroy(const VkDevice device) {
    vkDestroySampler(device, linearRepeat, nullptr);
    vkDestroySampler(device, linearClamp, nullptr);
    vkDestroySampler(device, nearestRepeat, nullptr);
    vkDestroySampler(device, nearestClamp, nullptr);
  }

  Renderer::~Renderer() {
    if (m.moveGuard.moved()) {
      return;
    }

    auto& device = m.vkcore.device.logical;
    auto& allocator = m.vkcore.allocator;

    vkDeviceWaitIdle(device);

    KT_VK_CONTEXT_DESTROY(m.tracyContext);

    for (auto& perFrame : m.vkcore.perFrame) {
      vkDestroySemaphore(device, perFrame.lightsFinished, nullptr);
      vkDestroySemaphore(device, perFrame.deferredRenderFinishedSemaphore, nullptr);
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

    m.samplers.destroy(device);

    m.pipelines.destroy(device);

    m.renderTargets.destroy(allocator, device);

    vmaDestroyAllocator(allocator);

    shutdownImGui();
    m.vkcore.swapchain.destroy();

    vkDeviceWaitIdle(device); // Sometimes complains that some of the swapchain resources havn't been destroyed yet.
    vkDestroyDevice(device, nullptr);

    vkDestroySurfaceKHR(m.vkcore.instance, m.vkcore.surface, nullptr);
    m.vkcore.instance.destroy();

    VK_INFO("Vulkan renderer shut down cleanly");
  }

} // namespace kt::vkh
