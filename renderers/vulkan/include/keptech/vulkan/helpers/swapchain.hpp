#pragma once

#include <expected>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {

  struct SwapchainConfig {
    ::vk::SurfaceFormatKHR format;
    ::vk::PresentModeKHR presentMode;
    ::vk::Extent2D extent;
    uint32_t minImageCount;
    uint32_t imageCount;
  };

  auto chooseSwapSurfaceFormat(
      const std::vector<::vk::SurfaceFormatKHR>& availableFormats) noexcept
      -> ::vk::SurfaceFormatKHR;
  auto chooseSwapPresentMode(
      const std::vector<::vk::PresentModeKHR>& availablePresentModes) noexcept
      -> ::vk::PresentModeKHR;

  auto chooseSwapExtent(int width, int height,
                        const ::vk::SurfaceCapabilitiesKHR& capabilities,
                        const bool waitOnZero = false) noexcept
      -> ::vk::Extent2D;

  uint32_t minImageCount(const ::vk::SurfaceCapabilitiesKHR& capabilities,
                         uint32_t desired) noexcept;

  uint32_t
  desiredImageCount(const vk::SurfaceCapabilitiesKHR& capabilities) noexcept;

  struct SwapchainQueues {
    uint32_t graphicsQueueIndex;
    uint32_t presentQueueIndex;
  };

  class Swapchain {

  public:
    Swapchain() = delete;

    static auto create(const vk::raii::Device& device,
                       const SwapchainConfig& swapchainConfig,
                       const vk::raii::PhysicalDevice& physicalDevice,
                       const vk::raii::SurfaceKHR& surface,
                       const SwapchainQueues& queues,
                       std::optional<vk::raii::SwapchainKHR*> oldSwapchain)
        -> std::expected<Swapchain, std::string>;

    [[nodiscard]] auto images() const noexcept
        -> const std::vector<vk::Image>& {
      return imgs;
    }

    [[nodiscard]] auto views() const noexcept
        -> const std::vector<vk::raii::ImageView>& {
      return imageViews;
    }

    [[nodiscard]] const SwapchainConfig& config() const noexcept {
      return _config;
    }

    [[nodiscard]] auto nImageView(const size_t index) const noexcept
        -> const vk::raii::ImageView& {
      return imageViews[index];
    }

    [[nodiscard]] auto nImage(const size_t index) const noexcept
        -> const vk::Image& {
      return imgs[index];
    }

    [[nodiscard]] auto nView(const size_t index) const noexcept
        -> const vk::raii::ImageView& {
      return imageViews[index];
    }

    [[nodiscard]] auto getSwapchain() const noexcept
        -> const vk::raii::SwapchainKHR& {
      return swapchain;
    }

    auto operator*() noexcept -> vk::raii::SwapchainKHR& { return swapchain; }
    auto operator*() const noexcept -> const vk::raii::SwapchainKHR& {
      return swapchain;
    }

    auto operator->() noexcept -> vk::raii::SwapchainKHR* { return &swapchain; }
    auto operator->() const noexcept -> const vk::raii::SwapchainKHR* {
      return &swapchain;
    }

    auto operator[](const size_t index) noexcept -> vk::raii::ImageView& {
      return imageViews[index];
    }
    auto operator[](const size_t index) const noexcept
        -> const vk::raii::ImageView& {
      return imageViews[index];
    }

    enum class State : uint8_t { Ok, Suboptimal, OutOfDate };

    struct AcquireResult {
      uint32_t imageIndex;
      State state;
    };

    [[nodiscard]] auto
    getNextImage(const vk::raii::Device& device,
                 const vk::raii::Fence& waitFence,
                 const vk::raii::Semaphore& signalSem) const noexcept
        -> std::expected<AcquireResult, std::string>;

  private:
    SwapchainConfig _config;
    vk::raii::SwapchainKHR swapchain;
    std::vector<vk::Image> imgs;
    std::vector<vk::raii::ImageView> imageViews;

    Swapchain(vk::raii::SwapchainKHR& swapchain, SwapchainConfig config,
              std::vector<vk::Image>& images,
              std::vector<vk::raii::ImageView>& imageViews) noexcept
        : _config(config), swapchain(std::move(swapchain)),
          imgs(std::move(images)), imageViews(std::move(imageViews)) {}
  };
} // namespace keptech::vkh
