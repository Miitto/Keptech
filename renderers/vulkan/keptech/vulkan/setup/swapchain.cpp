#include "keptech/vulkan/wrappers/swapchain.hpp"
#include "keptech/vulkan/core.hpp"
#include "macros.hpp"
#include <SDL3/SDL_vulkan.h>
#include <glm/glm.hpp>

namespace kt::vkh::setup {
  using namespace kt::vkh;

  std::expected<Swapchain, std::string> createSwapchain(const VkPhysicalDevice physicalDevice, glm::ivec2 framebufferSize,
                                                        const VkDevice device, const VkSurfaceKHR surface, const Queues& queues,
                                                        VkSwapchainKHR oldSwapchain) {
    VKH_MAKE(swapchain,
             Swapchain::create(device, Swapchain::CreateInfo{}, physicalDevice, surface,
                               {.graphicsQueueIndex = queues.graphics.index, .presentQueueIndex = queues.present.index}, oldSwapchain),
             "Failed to create swapchain");

    return std::move(swapchain);
  }

} // namespace kt::vkh::setup
