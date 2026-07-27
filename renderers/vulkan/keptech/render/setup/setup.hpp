#pragma once

#include "core.hpp"
#include "keptech/render/wrappers/fwd.hpp"
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

      std::expected<Swapchain, std::string> createSwapchain(const VkPhysicalDevice physicalDevice, const VkDevice device,
                                                            glm::uvec2 windowSize, const VkSurfaceKHR surface, const Queues& queues,
                                                            VkSwapchainKHR oldSwapchain);

    } // namespace setup
  } // namespace vkh
} // namespace kt