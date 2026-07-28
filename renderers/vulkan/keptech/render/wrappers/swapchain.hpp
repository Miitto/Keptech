#pragma once

#include "keptech/render/constants.hpp"
#include "keptech/render/macros.hpp"
#include <Volk/volk.h>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace kt::rdr {
  struct SwapchainConfig {
    VkSurfaceFormatKHR format = {.format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D extent = {};
    uint32_t imageCount = 0;
  };

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

    enum class State : uint8_t { Ok, Suboptimal, OutOfDate };

    struct AcquireResult {
      uint32_t imageIndex;
      State state;
    };

    Swapchain() = default;
    MOVE_ONLY(Swapchain)
    ~Swapchain();

    void destroy() noexcept;

    struct CreateInfo {
      bool vsync = true;
      bool desiredImgCount = MAX_FRAMES_IN_FLIGHT + 1;
      VkExtent2D extent = {};
    };

    static auto create(const VkDevice& device, const SwapchainConfig& swapchainConfig, const VkPhysicalDevice& physicalDevice,
                       const VkSurfaceKHR& surface, const SwapchainQueues& queues, VkSwapchainKHR oldSwapchain)
        -> std::expected<Swapchain, std::string>;

    static auto create(const VkDevice& device, const CreateInfo& info, const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface,
                       const SwapchainQueues& queues, VkSwapchainKHR oldSwapchain = nullptr) -> std::expected<Swapchain, std::string>;

    [[nodiscard]] auto images() const noexcept -> const std::vector<VkImage>& { return imgs; }
    [[nodiscard]] auto views() const noexcept -> const std::vector<VkImageView>& { return imageViews; }
    [[nodiscard]] auto nImage(const size_t imageIndex) const noexcept -> const VkImage& { return imgs[imageIndex]; }
    [[nodiscard]] auto nView(const size_t imageIndex) const noexcept -> const VkImageView& { return imageViews[imageIndex]; }
    [[nodiscard]] auto nPresentSemaphore(const size_t imageIndex) noexcept -> VkSemaphore& { return presentSemaphores[imageIndex]; }

    [[nodiscard]] VkImage getCurrentImage() const noexcept { return imgs[currentImageIndex]; }
    [[nodiscard]] VkImageView getCurrentImageView() const noexcept { return imageViews[currentImageIndex]; }
    [[nodiscard]] VkSemaphore getCurrentPresentSemaphore() const noexcept { return presentSemaphores[currentImageIndex]; }
    [[nodiscard]] const SwapchainConfig& config() const noexcept { return _config; }

    operator VkSwapchainKHR() const noexcept { return swapchain; }
    const VkSwapchainKHR& operator*() const noexcept { return swapchain; }

    [[nodiscard]] auto getNextImage(const VkDevice& device, VkFence& waitFence, VkSemaphore& signalSemaphore) noexcept
        -> std::expected<AcquireResult, std::string>;

  private:
    VkDevice device = VK_NULL_HANDLE;
    SwapchainConfig _config{};
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> imgs;
    std::vector<VkImageView> imageViews;
    std::vector<VkSemaphore> presentSemaphores;

    size_t currentImageIndex = 0;

    Swapchain(VkDevice device, VkSwapchainKHR swapchain, SwapchainConfig config, std::vector<VkImage>&& images,
              std::vector<VkImageView>&& imageViews, std::vector<VkSemaphore>&& sync) noexcept
        : device(device), _config(config), swapchain(swapchain), imgs(std::move(images)), imageViews(std::move(imageViews)),
          presentSemaphores(std::move(sync)) {}
  };
} // namespace kt::rdr
