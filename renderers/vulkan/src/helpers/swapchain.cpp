#include "keptech/vulkan/helpers/swapchain.hpp"

#include "vk-logger.hpp"
#include <macros.hpp>

namespace keptech::vkh {
  auto chooseSwapSurfaceFormat(
      const std::vector<vk::SurfaceFormatKHR>& availableFormats) noexcept
      -> vk::SurfaceFormatKHR {
    for (const auto& availableFormat : availableFormats) {
      if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
          availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
        return availableFormat;
      }
    }

    return availableFormats[0];
  }

  auto chooseSwapPresentMode(
      const std::vector<vk::PresentModeKHR>& availablePresentModes) noexcept
      -> vk::PresentModeKHR {
    for (const auto& availablePresentMode : availablePresentModes) {
      if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
        return availablePresentMode;
      }
    }
    return vk::PresentModeKHR::eFifo;
  }

  auto chooseSwapExtent(int width, int height,
                        const vk::SurfaceCapabilitiesKHR& capabilities,
                        const bool waitOnZero) noexcept -> vk::Extent2D {
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    }

    return {.width =
                std::clamp<uint32_t>(width, capabilities.minImageExtent.width,
                                     capabilities.maxImageExtent.width),
            .height =
                std::clamp<uint32_t>(height, capabilities.minImageExtent.height,
                                     capabilities.maxImageExtent.height)};
  }

  auto minImageCount(const vk::SurfaceCapabilitiesKHR& capabilities,
                     uint32_t desired) noexcept -> uint32_t {
    auto minImageCount = std::max(desired, capabilities.minImageCount);
    minImageCount = (capabilities.maxImageCount > 0 &&
                     minImageCount > capabilities.maxImageCount)
                        ? capabilities.maxImageCount
                        : minImageCount;

    return minImageCount;
  }

  auto Swapchain::create(const vk::raii::Device& device,
                         const SwapchainConfig& swapchainConfig,
                         const vk::raii::PhysicalDevice& physicalDevice,
                         const vk::raii::SurfaceKHR& surface,
                         const SwapchainQueues& queues,
                         std::optional<vk::raii::SwapchainKHR*> oldSwapchain)
      -> std::expected<Swapchain, std::string> {

    VK_MAKE(surfaceCapabilities,
            physicalDevice.getSurfaceCapabilitiesKHR(surface),
            "Failed to get surface capabilities");

    VK_DEBUG("Requested {} swapchain images", swapchainConfig.imageCount);

    vk::SwapchainCreateInfoKHR swapchainCreateInfo{
        .surface = *surface,
        .minImageCount = swapchainConfig.imageCount,
        .imageFormat = swapchainConfig.format.format,
        .imageColorSpace = swapchainConfig.format.colorSpace,
        .imageExtent = swapchainConfig.extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment |
                      vk::ImageUsageFlagBits::eTransferDst,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = swapchainConfig.presentMode,
        .clipped = VK_TRUE,
    };

    if (queues.graphicsQueueIndex != queues.presentQueueIndex) {
      swapchainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
      swapchainCreateInfo.queueFamilyIndexCount = 2;
      swapchainCreateInfo.pQueueFamilyIndices = &queues.graphicsQueueIndex;
    } else {
      swapchainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
      swapchainCreateInfo.queueFamilyIndexCount = 1;
      swapchainCreateInfo.pQueueFamilyIndices = &queues.graphicsQueueIndex;
    }

    if (oldSwapchain) {
      swapchainCreateInfo.oldSwapchain = **oldSwapchain;
    } else {
      swapchainCreateInfo.oldSwapchain = nullptr;
    }

    VK_MAKE(swapchain, device.createSwapchainKHR(swapchainCreateInfo),
            "Failed to create swapchain");

    VK_MAKE(images, swapchain.getImages(), "Failed to get swapchain images");

    VK_INFO("Swapchain created with {} images", images.size());

    vk::ImageViewCreateInfo imageViewCreateInfo{
        .viewType = vk::ImageViewType::e2D,
        .format = swapchainConfig.format.format,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};

    std::vector<vk::raii::ImageView> imageViews;
    std::vector<vk::raii::Semaphore> syncObjects;

    for (const auto& image : images) {
      imageViewCreateInfo.image = image;
      VK_MAKE(imageView, device.createImageView(imageViewCreateInfo),
              "Failed to create image view");
      imageViews.push_back(std::move(imageView));

      VK_MAKE(renderFinishedSemaphore, device.createSemaphore({}),
              "Failed to create semaphore");
      syncObjects.emplace_back(std::move(renderFinishedSemaphore));
    }

    Swapchain s(std::move(swapchain), swapchainConfig, std::move(images),
                std::move(imageViews), std::move(syncObjects));

    return s;
  }

  auto
  Swapchain::getNextImage(const vk::raii::Device& device,
                          vk::raii::Fence& waitFence,
                          vk::raii::Semaphore& signalSemaphore) const noexcept
      -> std::expected<AcquireResult, std::string> {
    while (vk::Result::eTimeout ==
           device.waitForFences({waitFence}, VK_TRUE, UINT64_MAX)) {
      // Wait for the queue to become idle
    }
    auto [result, index] = swapchain.acquireNextImage(
        std::numeric_limits<uint64_t>::max(), signalSemaphore, nullptr);

    if (result != vk::Result::eSuccess &&
        result != vk::Result::eSuboptimalKHR) {
      return std::unexpected("Failed to acquire next image: " +
                             vk::to_string(result));
    }

    if (result == vk::Result::eSuboptimalKHR) {
      return AcquireResult(index, State::Suboptimal);
    }

    if (result == vk::Result::eErrorOutOfDateKHR) {
      return AcquireResult(index, State::OutOfDate);
    }

    return AcquireResult(index, State::Ok);
  }

} // namespace keptech::vkh
