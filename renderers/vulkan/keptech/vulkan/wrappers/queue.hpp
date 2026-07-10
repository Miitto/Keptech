#pragma once

#include "cmdBuf.hpp"
#include "macros.hpp"
#include <Volk/volk.h>
#include <array>
#include <vector>

namespace kt::vkh {
  struct Queue {
    uint32_t index = ~0;
    VkQueue queue = nullptr;
  };

  struct CommandPool {
    VkCommandPool pool = nullptr;
    Queue queue{};

    [[nodiscard]] CommandBuffer allocate(const VkDevice& device) const {
      VkCommandBuffer commandBuffer = nullptr;
      VkCommandBufferAllocateInfo allocInfo{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          .commandPool = pool,
          .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          .commandBufferCount = 1,
      };
      VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer), "Failed to allocate command buffer");
      return commandBuffer;
    }

    template <uint32_t N>
    [[nodiscard]]
    std::array<CommandBuffer, N> allocate(const VkDevice& device) const {
      std::array<CommandBuffer, N> commandBuffers{};
      VkCommandBufferAllocateInfo allocInfo{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          .commandPool = pool,
          .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          .commandBufferCount = static_cast<uint32_t>(N),
      };
      VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, (VkCommandBuffer*)commandBuffers.data()), "Failed to allocate command buffers");
      return commandBuffers;
    }

    [[nodiscard]] std::vector<CommandBuffer> allocate(const VkDevice& device, uint32_t count) const {
      std::vector<CommandBuffer> commandBuffers(count);
      VkCommandBufferAllocateInfo allocInfo{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          .commandPool = pool,
          .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          .commandBufferCount = static_cast<uint32_t>(count),
      };
      VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, reinterpret_cast<VkCommandBuffer*>(commandBuffers.data())),
               "Failed to allocate command buffers");
      return commandBuffers;
    }
  };
} // namespace kt::vkh