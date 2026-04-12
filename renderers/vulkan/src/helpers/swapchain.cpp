#include "keptech/vulkan/helpers/swapchain.hpp"

#include "vk-logger.hpp"
#include <algorithm>
#include <array>
#include <macros.hpp>

namespace {
  struct PreferredFormat {
    VkFormat format;
    VkColorSpaceKHR colorSpace;
  };

  constexpr std::array PREFERRED_FORMATS{
      PreferredFormat{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
  };

  constexpr std::array PREFERRED_PRESENT_MODES{
      VK_PRESENT_MODE_MAILBOX_KHR,
  };
} // namespace

namespace kt::vkh {
  auto chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) noexcept -> VkSurfaceFormatKHR {
    for (auto& preferredFormat : PREFERRED_FORMATS) {
      for (auto& availableFormat : availableFormats) {
        if (availableFormat.format == preferredFormat.format && availableFormat.colorSpace == preferredFormat.colorSpace) {
          return availableFormat;
        }
      }
    }

    return availableFormats[0];
  }

  auto chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) noexcept -> VkPresentModeKHR {
    for (auto& preferredPresentMode : PREFERRED_PRESENT_MODES) {
      if (std::ranges::find(availablePresentModes, preferredPresentMode) != availablePresentModes.end()) {
        return preferredPresentMode;
      }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
  }

  auto chooseSwapExtent(int width, int height, const VkSurfaceCapabilitiesKHR& capabilities, const bool waitOnZero) noexcept -> VkExtent2D {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    }

    return {.width = std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            .height = std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
  }

  auto minImageCount(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t desired) noexcept -> uint32_t {
    auto minImageCount = std::max(desired, capabilities.minImageCount);
    minImageCount =
        (capabilities.maxImageCount > 0 && minImageCount > capabilities.maxImageCount) ? capabilities.maxImageCount : minImageCount;

    return minImageCount;
  }

  auto Swapchain::create(const VkDevice& device, const SwapchainConfig& swapchainConfig, const VkPhysicalDevice& physicalDevice,
                         const VkSurfaceKHR& surface, const SwapchainQueues& queues, VkSwapchainKHR oldSwapchain)
      -> std::expected<Swapchain, std::string> {

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VK_MAKE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities), "Failed to get surface capabilities");

    VK_DEBUG("Requested {} swapchain images", swapchainConfig.imageCount);

    VkSwapchainCreateInfoKHR swapchainCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = swapchainConfig.imageCount,
        .imageFormat = swapchainConfig.format.format,
        .imageColorSpace = swapchainConfig.format.colorSpace,
        .imageExtent = swapchainConfig.extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = swapchainConfig.presentMode,
        .clipped = VK_TRUE,
    };

    if (queues.graphicsQueueIndex != queues.presentQueueIndex) {
      swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      swapchainCreateInfo.queueFamilyIndexCount = 2;
      swapchainCreateInfo.pQueueFamilyIndices = &queues.graphicsQueueIndex;
    } else {
      swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      swapchainCreateInfo.queueFamilyIndexCount = 1;
      swapchainCreateInfo.pQueueFamilyIndices = &queues.graphicsQueueIndex;
    }

    swapchainCreateInfo.oldSwapchain = oldSwapchain;

    VkSwapchainKHR swapchain;
    VK_MAKE(vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain), "Failed to create swapchain");

    uint32_t imageCount;
    VK_MAKE(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr), "Failed to get swapchain images count");
    std::vector<VkImage> images(imageCount);
    VK_MAKE(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data()), "Failed to get swapchain images");

    VK_INFO("Swapchain created with {} images", images.size());

    VkImageViewCreateInfo imageViewCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = swapchainConfig.format.format,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    VkSemaphoreCreateInfo semaphoreCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    std::vector<VkImageView> imageViews(imageCount);
    std::vector<VkSemaphore> renderFinishedSemaphores(imageCount);

    for (size_t i = 0; i < imageCount; i++) {
      imageViewCreateInfo.image = images[i];
      VK_MAKE(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageViews[i]), "Failed to create image view for swapchain image");

      VK_MAKE(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphores[i]),
              "Failed to create render finished semaphore for swapchain image");
    }

    Swapchain s(device, swapchain, swapchainConfig, std::move(images), std::move(imageViews), std::move(renderFinishedSemaphores));

    return s;
  }

  auto Swapchain::getNextImage(const VkDevice& device, VkFence& waitFence, VkSemaphore& signalSemaphore) const noexcept
      -> std::expected<AcquireResult, std::string> {
    while (VK_TIMEOUT == vkWaitForFences(device, 1, &waitFence, VK_TRUE, UINT64_MAX)) {
      // Doubt will ever be hit
      VK_WARN("Wait for fence timed out while acquiring next swapchain image. This should not happen, but if it does, it means the GPU is "
              "taking a very long time to render a frame. Yielding thread to avoid busy waiting.");
      std::this_thread::yield();
    }
    vkResetFences(device, 1, &waitFence);
    uint32_t index = 0;
    auto result = vkAcquireNextImageKHR(device, swapchain, std::numeric_limits<uint64_t>::max(), signalSemaphore, VK_NULL_HANDLE, &index);

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      return std::unexpected("Failed to acquire next image");
    }

    if (result == VK_SUBOPTIMAL_KHR) {
      return AcquireResult(index, State::Suboptimal);
    }

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      return AcquireResult(index, State::OutOfDate);
    }

    return AcquireResult(index, State::Ok);
  }
  Swapchain::Swapchain(Swapchain&& other) noexcept
      : device(other.device), swapchain(other.swapchain), imgs(std::move(other.imgs)), imageViews(std::move(other.imageViews)),
        presentSemaphores(std::move(other.presentSemaphores)), _config(other._config) {
    other.swapchain = VK_NULL_HANDLE;
  }

  Swapchain& Swapchain::operator=(Swapchain&& other) noexcept {
    if (this != &other) {
      device = other.device;
      swapchain = other.swapchain;
      imgs = std::move(other.imgs);
      imageViews = std::move(other.imageViews);
      presentSemaphores = std::move(other.presentSemaphores);
      _config = other._config;

      other.swapchain = VK_NULL_HANDLE;
    }
    return *this;
  }

  void Swapchain::destroy() noexcept {
    for (auto& semaphore : presentSemaphores) {
      vkDestroySemaphore(device, semaphore, nullptr);
    }
    for (auto& imageView : imageViews) {
      vkDestroyImageView(device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain, nullptr);
  }

  Swapchain::~Swapchain() {
    if (swapchain != VK_NULL_HANDLE) {
      destroy();
    }
  }
} // namespace kt::vkh
