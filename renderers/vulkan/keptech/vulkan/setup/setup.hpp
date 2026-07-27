#pragma once

#include "keptech/vulkan/structs.hpp"
#include "keptech/vulkan/wrappers/fwd.hpp"

#include "core.hpp"
#include "keptech/vulkan/constants.hpp"
#include <Volk/volk.h>
#include <expected>
#include <glm/fwd.hpp>

namespace kt::core::window {
  class Window;
}

namespace kt {
  struct RendererCreateInfo;
  namespace vkh {
    class Renderer;
    struct VulkanCore;
    struct Buffers;
    struct RenderTargets;
    struct Samplers;
    struct StaticDescriptors;
    struct Members;

    namespace setup {
      struct QueueIndices {
        uint32_t graphics = std::numeric_limits<uint32_t>::max();
        uint32_t present = std::numeric_limits<uint32_t>::max();
        uint32_t compute = std::numeric_limits<uint32_t>::max();
        uint32_t transfer = std::numeric_limits<uint32_t>::max();
      };

      void writeGlobalDescriptors(Members& m, DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT>& sets);
      std::expected<StaticDescriptors, std::string> createStaticDescriptors(const VulkanCore& vkcore);
      void writeStaticDescriptors(const VulkanCore& vkcore, const StaticDescriptors& staticDescriptorSets, const Buffers& buffers);
      std::expected<Swapchain, std::string> createSwapchain(const VkPhysicalDevice physicalDevice, const VkDevice device,
                                                            glm::uvec2 windowSize, const VkSurfaceKHR surface, const Queues& queues,
                                                            VkSwapchainKHR oldSwapchain);

    } // namespace setup
  } // namespace vkh
} // namespace kt