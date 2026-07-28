#include "keptech/render/wrappers/swapchain.hpp"
#include "keptech/render/core.hpp"
#include "macros.hpp"
#include <SDL3/SDL_vulkan.h>
#include <glm/glm.hpp>

namespace kt::rdr::setup {
  using namespace kt::rdr;

  std::expected<Swapchain, std::string> createSwapchain(const VkPhysicalDevice physicalDevice, const VkDevice device, glm::uvec2 windowSize,
                                                        const VkSurfaceKHR surface, const Queues& queues, VkSwapchainKHR oldSwapchain) {
    Swapchain::CreateInfo createInfo{};
    createInfo.extent.width = windowSize.x;
    createInfo.extent.height = windowSize.y;
    VKH_MAKE(swapchain,
             Swapchain::create(device, createInfo, physicalDevice, surface,
                               {.graphicsQueueIndex = queues.graphics.index, .presentQueueIndex = queues.present.index}, oldSwapchain),
             "Failed to create swapchain");

    return std::move(swapchain);
  }

} // namespace kt::rdr::setup
