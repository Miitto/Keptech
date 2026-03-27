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
  std::expected<DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT>, std::string> createGlobalDescriptors(VkDevice device);
  std::expected<VkPhysicalDevice, std::string> createPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
  std::expected<QueueIndices, std::string> findQueues(VkPhysicalDevice physDevice, VkSurfaceKHR surface);
  std::expected<VkDevice, std::string> createDevice(VkPhysicalDevice physDevice, const std::set<uint32_t>& uniqueQueueFamilies);
  std::expected<Queues, std::string> getQueues(VkDevice& device, QueueIndices& queueIndices, const std::set<uint32_t>& uniqueQueueFamilies);
  std::expected<Renderer::ImGuiVkObjects, std::string> setupImGui(const kt::core::window::Window& window,
                                                                  const Renderer::VulkanCore& vkcore);
  std::expected<Swapchain, std::string> createSwapchain(const VkPhysicalDevice physicalDevice, glm::ivec2 framebufferSize,
                                                        const VkDevice device, const VkSurfaceKHR surface, const Queues& queues,
                                                        VkSwapchainKHR oldSwapchain);
  std::expected<Renderer::Pipelines, std::string> createPipelines(const Renderer::VulkanCore& vkcore);
  std::expected<Renderer::Buffers, std::string> createBuffers(const Renderer::VulkanCore& vkcore);
} // namespace kt::vkh::setup
