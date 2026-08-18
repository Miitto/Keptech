#pragma once

#include "keptech/rhi/constants.hpp"
#include "keptech/rhi/device.hpp"
#include "keptech/rhi/image.hpp"
#include "keptech/rhi/instance.hpp"
#include "keptech/rhi/queue.hpp"
#include "keptech/rhi/swapchain.hpp"
#include <Volk/volk.h>
#include <array>
#include <vma/vk_mem_alloc.h>

namespace kt::rhi {
  struct Queues {
    Queue graphics;
    Queue present;
    Queue compute;
    Queue transfer;
  };

  struct Pools {
    CommandPool graphics{};
    CommandPool compute{};

    void resetAll();
  };

  struct TextureUpdateInfo {
    Image* texture = nullptr;
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
} // namespace kt::rhi