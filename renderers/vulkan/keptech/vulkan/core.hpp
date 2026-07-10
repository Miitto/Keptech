#pragma once

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

  struct VulkanCore {
    Instance instance;
    VkSurfaceKHR surface;
    Device device;
    VmaAllocator allocator;
    Queues queues;
    Swapchain swapchain;
    std::array<PerFrame, MAX_FRAMES_IN_FLIGHT> perFrame;
    CommandPool transferPool;
    VkSemaphore timelineSemaphore;
    uint64_t timelineValue = 0;
  };
} // namespace kt::vkh