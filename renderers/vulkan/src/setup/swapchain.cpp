#pragma once

#include "keptech/vulkan/helpers/swapchain.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include <SDL3/SDL_vulkan.h>

namespace kt::vkh::setup {
  using namespace kt::vkh;

  std::expected<Swapchain, std::string> createSwapchain(const VkPhysicalDevice physicalDevice, glm::ivec2 framebufferSize,
                                                        const VkDevice device, const VkSurfaceKHR surface, const Queues& queues,
                                                        VkSwapchainKHR oldSwapchain) {
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VK_MAKE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities), "Failed to get surface capabilities");

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfaceFormats.data());
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());

    auto format = chooseSwapSurfaceFormat(surfaceFormats);
    auto presentMode = chooseSwapPresentMode(presentModes);

    auto extent = chooseSwapExtent(framebufferSize.x, framebufferSize.y, surfaceCapabilities, true);
    auto minImageCount = kt::vkh::minImageCount(surfaceCapabilities, MAX_FRAMES_IN_FLIGHT);

    VK_DEBUG("Min image count for swapchain: {}", minImageCount);

    SwapchainConfig swapchainConfig{.format = format, .presentMode = presentMode, .extent = extent, .imageCount = minImageCount};

    VKH_MAKE(swapchain,
             Swapchain::create(device, swapchainConfig, physicalDevice, surface,
                               {.graphicsQueueIndex = queues.graphics.index, .presentQueueIndex = queues.present.index}, oldSwapchain),
             "Failed to create swapchain");

    return std::move(swapchain);
  }

} // namespace kt::vkh::setup
