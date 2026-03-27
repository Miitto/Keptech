#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace kt::vkh {

  struct SwapchainConfig {
    VkSurfaceFormatKHR format;
    VkPresentModeKHR presentMode;
    VkExtent2D extent;
    uint32_t imageCount;
  };

  auto chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) noexcept -> VkSurfaceFormatKHR;
  auto chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) noexcept -> VkPresentModeKHR;

  auto chooseSwapExtent(int width, int height, const VkSurfaceCapabilitiesKHR& capabilities, const bool waitOnZero = false) noexcept
      -> VkExtent2D;

  uint32_t minImageCount(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t desired) noexcept;

  struct SwapchainQueues {
    uint32_t graphicsQueueIndex;
    uint32_t presentQueueIndex;
  };

  class Swapchain {

  public:
    struct Sync {
      VkSemaphore imageAvailableSemaphore;
      VkSemaphore renderFinishedSemaphore;
    };

    Swapchain() = delete;
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain(Swapchain&&) noexcept;
    Swapchain& operator=(Swapchain&&) noexcept;
    ~Swapchain();

    static auto create(const VkDevice& device, const SwapchainConfig& swapchainConfig, const VkPhysicalDevice& physicalDevice,
                       const VkSurfaceKHR& surface, const SwapchainQueues& queues, VkSwapchainKHR oldSwapchain)
        -> std::expected<Swapchain, std::string>;

    [[nodiscard]] auto images() const noexcept -> const std::vector<VkImage>& { return imgs; }

    [[nodiscard]] auto views() const noexcept -> const std::vector<VkImageView>& { return imageViews; }

    [[nodiscard]] const SwapchainConfig& config() const noexcept { return _config; }

    [[nodiscard]] auto nImage(const size_t imageIndex) const noexcept -> const VkImage& { return imgs[imageIndex]; }

    [[nodiscard]] auto nView(const size_t imageIndex) const noexcept -> const VkImageView& { return imageViews[imageIndex]; }

    [[nodiscard]] auto nPresentSemaphore(const size_t imageIndex) noexcept -> VkSemaphore& { return presentSemaphores[imageIndex]; }

    [[nodiscard]] auto getSwapchain() noexcept -> VkSwapchainKHR& { return swapchain; }

    [[nodiscard]] auto getSwapchain() const noexcept -> const VkSwapchainKHR& { return swapchain; }

    auto operator*() noexcept -> VkSwapchainKHR& { return swapchain; }
    auto operator*() const noexcept -> const VkSwapchainKHR& { return swapchain; }

    auto operator->() noexcept -> VkSwapchainKHR* { return &swapchain; }
    auto operator->() const noexcept -> const VkSwapchainKHR* { return &swapchain; }

    auto operator[](const size_t index) noexcept -> VkImageView& { return imageViews[index]; }
    auto operator[](const size_t index) const noexcept -> const VkImageView& { return imageViews[index]; }

    enum class State : uint8_t { Ok, Suboptimal, OutOfDate };

    struct AcquireResult {
      uint32_t imageIndex;
      State state;
    };

    [[nodiscard]] auto getNextImage(const VkDevice& device, VkFence& waitFence, VkSemaphore& signalSemaphore) const noexcept
        -> std::expected<AcquireResult, std::string>;

  private:
    VkDevice device;
    SwapchainConfig _config;
    VkSwapchainKHR swapchain;
    std::vector<VkImage> imgs;
    std::vector<VkImageView> imageViews;
    std::vector<VkSemaphore> presentSemaphores;

    Swapchain(VkDevice device, VkSwapchainKHR swapchain, SwapchainConfig config, std::vector<VkImage>&& images,
              std::vector<VkImageView>&& imageViews, std::vector<VkSemaphore>&& sync) noexcept
        : device(device), _config(config), swapchain(swapchain), imgs(std::move(images)), imageViews(std::move(imageViews)),
          presentSemaphores(std::move(sync)) {}
  };
} // namespace kt::vkh
