#pragma once

#include "keptech/vulkan/renderer.hpp"

#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include <SDL3/SDL_vulkan.h>
#include <expected>
#include <keptech/core/components/camera.hpp>

namespace keptech::vkh::setup {
  using namespace keptech::vkh;

  std::expected<Swapchain, std::string>
  createSwapchain(const vk::raii::PhysicalDevice& physicalDevice,
                  glm::ivec2 framebufferSize, const vk::raii::Device& device,
                  const vk::raii::SurfaceKHR& surface,
                  const Renderer::Queues& queues,
                  std::optional<vk::raii::SwapchainKHR*> oldSwapchain) {
    VK_MAKE(surfaceCapabilities,
            physicalDevice.getSurfaceCapabilitiesKHR(surface),
            "Failed to get surfacce capabilities");

    VK_MAKE(surfaceFormats, physicalDevice.getSurfaceFormatsKHR(surface),
            "Failed to get surface formats");
    VK_MAKE(presentModes, physicalDevice.getSurfacePresentModesKHR(surface),
            "Failed to get surface present modes");

    auto format = chooseSwapSurfaceFormat(surfaceFormats);
    auto presentMode = chooseSwapPresentMode(presentModes);

    auto extent = chooseSwapExtent(framebufferSize.x, framebufferSize.y,
                                   surfaceCapabilities, true);
    auto minImageCount =
        keptech::vkh::minImageCount(surfaceCapabilities, MAX_FRAMES_IN_FLIGHT);

    VK_DEBUG("Min image count for swapchain: {}", minImageCount);

    SwapchainConfig swapchainConfig{.format = format,
                                    .presentMode = presentMode,
                                    .extent = extent,
                                    .imageCount = minImageCount};

    VKH_MAKE(swapchain,
             Swapchain::create(device, swapchainConfig, physicalDevice, surface,
                               {.graphicsQueueIndex = queues.graphics.index,
                                .presentQueueIndex = queues.present.index},
                               oldSwapchain),
             "Failed to create swapchain");

    return std::move(swapchain);
  }
} // namespace keptech::vkh::setup
