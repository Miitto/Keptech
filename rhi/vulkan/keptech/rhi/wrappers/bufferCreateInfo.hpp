#pragma once

namespace kt::rhi {
  class BufferCreateInfo {
  public:
    constexpr BufferCreateInfo(size_t size, VkBufferUsageFlags usage, VmaAllocationCreateFlags allocFlags, VmaMemoryUsage memUsage,
                               const char* name = nullptr) noexcept
        : bufferInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                     .pNext = nullptr,
                     .flags = 0,
                     .size = size,
                     .usage = usage,
                     .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                     .queueFamilyIndexCount = 0,
                     .pQueueFamilyIndices = nullptr},
          allocInfo{
              .flags = allocFlags,
              .usage = memUsage,
              .requiredFlags = 0,
              .preferredFlags = 0,
              .memoryTypeBits = 0,
              .pool = nullptr,
              .pUserData = nullptr,
              .priority = 0.f,
          },
          name(name) {}

    void setSize(size_t size) noexcept { bufferInfo.size = size; }
    void setUsage(VkBufferUsageFlags usage) noexcept { bufferInfo.usage = usage; }
    void setAllocFlags(VmaAllocationCreateFlags flags) noexcept { allocInfo.flags = flags; }
    void setMemoryUsage(VmaMemoryUsage usage) noexcept { allocInfo.usage = usage; }

    [[nodiscard]]
    const VkBufferCreateInfo& getBufferInfo() const noexcept {
      return bufferInfo;
    }
    [[nodiscard]]
    const VmaAllocationCreateInfo& getAllocInfo() const noexcept {
      return allocInfo;
    }
    [[nodiscard]]
    const char* getName() const noexcept {
      return name;
    }

  private:
    VkBufferCreateInfo bufferInfo;
    VmaAllocationCreateInfo allocInfo;
    const char* name = nullptr;
  };
} // namespace kt::rhi