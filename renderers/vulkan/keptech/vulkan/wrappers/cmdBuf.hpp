#pragma once

#include "keptech/vulkan/macros.hpp"
#include <Volk/volk.h>

namespace kt::vkh {
  class CommandBuffer {
  public:
    CommandBuffer() = default;
    constexpr CommandBuffer(VkCommandBuffer cmdBuf) : cmdBuf(cmdBuf) {}

    void begin() const {
      VkCommandBufferBeginInfo beginInfo{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
          .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      };
      VK_CHECK(vkBeginCommandBuffer(cmdBuf, &beginInfo), "Failed to begin command buffer");
    }

    void barrier(const VkDependencyInfo& dependencyInfo) const { vkCmdPipelineBarrier2(cmdBuf, &dependencyInfo); }

    void beginRendering(const VkRenderingInfo& renderingInfo) const { vkCmdBeginRendering(cmdBuf, &renderingInfo); }
    void endRendering() const { vkCmdEndRendering(cmdBuf); }

    void end() const { VK_CHECK(vkEndCommandBuffer(cmdBuf), "Failed to end command buffer"); }

    [[nodiscard]] constexpr operator VkCommandBuffer() const { return cmdBuf; }
    [[nodiscard]] constexpr VkCommandBuffer get() const { return cmdBuf; }
    [[nodiscard]] constexpr VkCommandBuffer operator*() const { return cmdBuf; }

    void label(const VkDevice device, const std::string& name) const { label(device, name.c_str()); }

    void label(const VkDevice device, const char* name) const {
#ifndef NDEBUG
      VkDebugUtilsObjectNameInfoEXT nameInfo{
          .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
          .objectType = VK_OBJECT_TYPE_COMMAND_BUFFER,
          .objectHandle = reinterpret_cast<uint64_t>(cmdBuf),
          .pObjectName = name,
      };
      vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
#endif
    }

  private:
    VkCommandBuffer cmdBuf;
  };
} // namespace kt::vkh