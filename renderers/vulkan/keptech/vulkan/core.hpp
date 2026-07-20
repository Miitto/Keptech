#pragma once

#include "keptech/vulkan/constants.hpp"
#include "keptech/vulkan/helpers/owned.hpp"
#include "keptech/vulkan/wrappers/buffer.hpp"
#include "keptech/vulkan/wrappers/device.hpp"
#include "keptech/vulkan/wrappers/image.hpp"
#include "keptech/vulkan/wrappers/instance.hpp"
#include "keptech/vulkan/wrappers/queue.hpp"
#include "keptech/vulkan/wrappers/swapchain.hpp"
#include <Volk/volk.h>
#include <array>
#include <vma/vk_mem_alloc.h>

namespace kt::vkh {
  struct Queues {
    Queue graphics;
    Queue present;
    Queue compute;
    Queue transfer;
  };

  struct Pools {
    CommandPool graphics{};
    CommandPool compute{};

    void resetAll(VkDevice device);
  };

  struct TextureUpdateInfo {
    Image texture;
    size_t indexInDescriptorSet = 0;
  };

  struct PerFrame {
    VkFence inFlightFence;
    VkSemaphore imageAvailableSemaphore;
    Pools pools;
    std::vector<TextureUpdateInfo> texToUpdate;
  };

  struct TimelineSemaphore {
    VkSemaphore semaphore = nullptr;
    uint64_t value = 0;
  };

  struct VulkanCore {
    Instance instance;
    VkSurfaceKHR surface;
    Device device;
    VmaAllocator allocator;
    Queues queues;
    Swapchain swapchain;
    TimelineSemaphore mainSemaphore;
    std::array<PerFrame, MAX_FRAMES_IN_FLIGHT> perFrame;
    CommandPool transferPool;
    TimelineSemaphore transferSemaphore;
  };
} // namespace kt::vkh