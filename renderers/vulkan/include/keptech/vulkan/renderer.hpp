#pragma once

#include "keptech/core/moveGuard.hpp"
#include "keptech/vulkan/helpers/device.hpp"
#include "keptech/vulkan/helpers/swapchain.hpp"
#include <expected>
#include <functional>
#include <keptech/vulkan/structs.hpp>
#include <memory>
#include <string>
#include <vk_mem_alloc.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::core::window {
  class Window;
}

namespace keptech::vkh {
  class Renderer {
  public:
    struct Queues {
      Queue graphics;
      Queue present;
      Queue compute;
      Queue transfer;
    };

    struct Pools {
      std::shared_ptr<vk::raii::CommandPool> graphics;
      std::shared_ptr<vk::raii::CommandPool> present;
      std::shared_ptr<vk::raii::CommandPool> compute;
      std::shared_ptr<vk::raii::CommandPool> transfer;

      void resetAll();
    };

    struct SyncObjects {
      vk::raii::Semaphore presentCompleteSemaphore;
      vk::raii::Semaphore renderCompleteSemaphore;
      vk::raii::Fence drawingFence;
    };

    struct FrameResources {
      SyncObjects syncObjects;
      Pools pools;
    };

    struct OldSwapchain {
      vkh::Swapchain swapchain;
      uint8_t frameIndex;
    };

    struct VulkanCore {
      vk::raii::Context context;
      vk::raii::Instance instance;
      vk::raii::SurfaceKHR surface;
      Device device;
      Queues queues;
      Swapchain swapchain;
      std::array<FrameResources, MAX_FRAMES_IN_FLIGHT> frameResources;

      std::optional<OldSwapchain> oldSwapchain = std::nullopt;
    };

    struct ImGuiVkObjects {
      vk::raii::DescriptorPool descriptorPool;
    };

    struct Frame {
      constexpr static uint8_t INVALID_INDEX = 255;

      uint8_t index = INVALID_INDEX;
      uint8_t imageIndex = INVALID_INDEX;
      std::reference_wrapper<SyncObjects> syncObjects;
      std::reference_wrapper<Pools> pools;
    };

  private:
    Renderer(core::window::Window& window, VulkanCore&& vkcore,
             vma::Allocator& allocator, ImGuiVkObjects&& imGuiObjects)
        : window(&window), vkcore(std::move(vkcore)), allocator(allocator),
          imGuiObjects(std::move(imGuiObjects)) {}

  public:
    std::expected<Renderer, std::string> static create(
        const char* const name, core::window::Window& window);

    Renderer() = delete;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) noexcept = default;
    Renderer& operator=(Renderer&&) = default;

    void newFrame();

    void render();

    ~Renderer();

  private:
    void checkSwapchain();
    std::expected<void, std::string> recreateSwapchain();

    Frame startFrame();
    void draw(const Frame& info,
              const vk::raii::CommandBuffer& graphicsCmdBuffer);
    void drawImGui(const Frame& info,
                   const vk::raii::CommandBuffer& graphicsCmdBuffer);
    void presentFrame(const Frame& info);
    void endFrame();

    inline void registerCommandBuffer(uint8_t frameIndex,
                                      vk::raii::CommandBuffer&& commandBuffer) {
      submittedCommandBuffers[frameIndex].emplace_back(
          std::move(commandBuffer));
    }

    core::MoveGuard moveGuard = core::MoveGuard{};

    core::window::Window* window;
    VulkanCore vkcore;
    vma::Allocator allocator;
    ImGuiVkObjects imGuiObjects;
    std::array<std::vector<vk::raii::CommandBuffer>, MAX_FRAMES_IN_FLIGHT>
        submittedCommandBuffers;

    uint8_t frameIndex = 0;
  };

  namespace setup {
    std::expected<Swapchain, std::string>
    createSwapchain(const vk::raii::PhysicalDevice& physicalDevice,
                    glm::ivec2 framebufferSize, const vk::raii::Device& device,
                    const vk::raii::SurfaceKHR& surface,
                    const Renderer::Queues& queues,
                    std::optional<vk::raii::SwapchainKHR*> oldSwapchain);
  }
} // namespace keptech::vkh
