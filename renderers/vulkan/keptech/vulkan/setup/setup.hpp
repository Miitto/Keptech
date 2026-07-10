#pragma once

#include "keptech/vulkan/structs.hpp"
#include "keptech/vulkan/wrappers/fwd.hpp"

#include "core.hpp"
#include <Volk/volk.h>
#include <expected>
#include <glm/fwd.hpp>
#include <set>

namespace kt::core {
  namespace window {
    class Window;
  }
} // namespace kt::core

namespace kt {
  struct RendererCreateInfo;
  namespace vkh {
    class Renderer;
    struct VulkanCore;
    struct Buffers;
    struct RenderTargets;
    struct Samplers;
    struct Layouts;
    struct Pipelines;
    struct StaticDescriptors;
    namespace setup {
      struct QueueIndices {
        uint32_t graphics = std::numeric_limits<uint32_t>::max();
        uint32_t present = std::numeric_limits<uint32_t>::max();
        uint32_t compute = std::numeric_limits<uint32_t>::max();
        uint32_t transfer = std::numeric_limits<uint32_t>::max();
      };

      std::expected<std::array<Pools, 2>, std::string>
      createPools(std::set<uint32_t>& uniqueQueueFamilies, const QueueIndices& queueIndices, VkDevice& device, const Queues& queues);
      std::expected<VulkanCore, std::string> createVulkanCore(const RendererCreateInfo& createInfo, const core::window::Window& window);
      std::expected<Formats, std::string> findFormats(const VulkanCore& vkcore);
      std::expected<DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT>, std::string> createGlobalDescriptors(VkDevice device);
      void writeGlobalDescriptors(const VulkanCore& vkcore, DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT>& sets, Buffers& buffers);
      std::expected<StaticDescriptors, std::string> createStaticDescriptors(const VulkanCore& vkcore);
      void writeStaticDescriptors(const VulkanCore& vkcore, const StaticDescriptors& staticDescriptorSets, const Buffers& buffers,
                                  const RenderTargets& renderTargets, const Samplers& samplers);
      std::expected<VkPhysicalDevice, std::string> createPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
      std::expected<QueueIndices, std::string> findQueues(VkPhysicalDevice physDevice, VkSurfaceKHR surface);
      std::expected<VkDevice, std::string> createDevice(VkPhysicalDevice physDevice, const std::set<uint32_t>& uniqueQueueFamilies);
      std::expected<Queues, std::string> getQueues(VkDevice& device, QueueIndices& queueIndices,
                                                   const std::set<uint32_t>& uniqueQueueFamilies);
      std::expected<Samplers, std::string> createSamplers(VkDevice device);
      std::expected<VkDescriptorPool, std::string> setupImGui(const kt::core::window::Window& window, const VulkanCore& vkcore,
                                                              const Samplers& samplers);
      std::expected<Swapchain, std::string> createSwapchain(const VkPhysicalDevice physicalDevice, glm::ivec2 framebufferSize,
                                                            const VkDevice device, const VkSurfaceKHR surface, const Queues& queues,
                                                            VkSwapchainKHR oldSwapchain);

      std::expected<Buffers, std::string> createBuffers(const VulkanCore& vkcore);
      std::expected<Layouts, std::string> createLayouts(const VkDevice device, const VkDescriptorSetLayout globalLayout,
                                                        const VkDescriptorSetLayout staticLayout);
      std::expected<Pipelines, std::string> createPipelines(const VulkanCore& vkcore, const Formats& formats, const Layouts& layouts);

      std::expected<RenderTargets, std::string> createRenderTargets(const VulkanCore& vkcore, const Formats& formats,
                                                                    const glm::ivec2& framebufferSize);
      std::expected<void, std::string> writeSsao(const VulkanCore& vkcore, const Buffers& buffers, const RenderTargets& renderTargets);
    } // namespace setup
  } // namespace vkh
} // namespace kt