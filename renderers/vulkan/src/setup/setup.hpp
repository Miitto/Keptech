#pragma once

#include "keptech/vulkan/renderer.hpp"
#include "keptech/vulkan/structs.hpp"
#include <expected>
#include <set>
#include <vulkan/vulkan.h>

namespace kt::vkh::setup {
  struct QueueIndices {
    uint32_t graphics;
    uint32_t present;
    uint32_t compute = std::numeric_limits<uint32_t>::max();
    uint32_t transfer = std::numeric_limits<uint32_t>::max();
  };

  std::expected<std::array<Pools, 2>, std::string> createPools(std::set<uint32_t>& uniqueQueueFamilies, const QueueIndices& queueIndices,
                                                               VkDevice& device, const Queues& queues);
  std::expected<Renderer::VulkanCore, std::string> createVulkanCore(const RendererCreateInfo& createInfo,
                                                                    const core::window::Window& window);
  std::expected<Formats, std::string> findFormats(const Renderer::VulkanCore& vkcore);
  std::expected<DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT>, std::string> createGlobalDescriptors(VkDevice device);
  std::expected<Renderer::StaticDescriptors, std::string> createStaticDescriptors(const Renderer::VulkanCore& vkcore);
  std::expected<VkPhysicalDevice, std::string> createPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
  std::expected<QueueIndices, std::string> findQueues(VkPhysicalDevice physDevice, VkSurfaceKHR surface);
  std::expected<VkDevice, std::string> createDevice(VkPhysicalDevice physDevice, const std::set<uint32_t>& uniqueQueueFamilies);
  std::expected<Queues, std::string> getQueues(VkDevice& device, QueueIndices& queueIndices, const std::set<uint32_t>& uniqueQueueFamilies);
  std::expected<Renderer::Samplers, std::string> createSamplers(VkDevice device);
  std::expected<VkDescriptorPool, std::string> setupImGui(const kt::core::window::Window& window, const Renderer::VulkanCore& vkcore,
                                                          const Renderer::Samplers& samplers);
  std::expected<Swapchain, std::string> createSwapchain(const VkPhysicalDevice physicalDevice, glm::ivec2 framebufferSize,
                                                        const VkDevice device, const VkSurfaceKHR surface, const Queues& queues,
                                                        VkSwapchainKHR oldSwapchain);

  std::expected<Renderer::Buffers, std::string> createBuffers(const Renderer::VulkanCore& vkcore);
  std::expected<Renderer::Pipelines, std::string> createPipelines(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                                  const VkDescriptorSetLayout globalLayout,
                                                                  const VkDescriptorSetLayout staticLayout);

  std::expected<Renderer::RenderTargets, std::string> createRenderTargets(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                                          const glm::ivec2& framebufferSize);
  std::expected<void, std::string> writeSsao(const Renderer::VulkanCore& vkcore, const Renderer::Buffers& buffers,
                                             const Renderer::RenderTargets& renderTargets);
} // namespace kt::vkh::setup
