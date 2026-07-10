#pragma once

#include "keptech/vulkan/vk-logger.hpp"
#include <Volk/volk.h>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <vma/vk_mem_alloc.h>

namespace kt::vkh {

  class Buffer {
  public:
    Buffer() = default;
    [[nodiscard]] bool isMapped() const { return allocInfo.pMappedData != nullptr; }
    [[nodiscard]] uint8_t* mapping(VkDeviceSize offset = 0) const { return static_cast<uint8_t*>(allocInfo.pMappedData) + offset; }

    void setDebugName(const VkDevice& device, const std::string& name) const {}
    [[nodiscard]] size_t size() const { return allocInfo.size; }

    operator VkBuffer() const { return buffer; }
    operator VkDeviceAddress() const { return gpuAddress; }

    [[nodiscard]] VkDeviceAddress address() const { return gpuAddress; }

    static std::expected<Buffer, std::string> create(VkDevice device, const VmaAllocator& allocator, VkBufferCreateInfo bufInfo,
                                                     const VmaAllocationCreateInfo& allocInfo, const std::string& name);

    void destroy(const VmaAllocator& allocator);

  private:
    Buffer(VkBuffer buffer, VmaAllocation alloc, VmaAllocationInfo allocInfo, VkDeviceAddress gpuAddress)
        : buffer(buffer), alloc(alloc), allocInfo(allocInfo), destroyed(std::make_shared<bool>(false)), gpuAddress(gpuAddress) {}

    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation alloc = nullptr;
    VmaAllocationInfo allocInfo = {};
    std::shared_ptr<bool> destroyed = std::make_shared<bool>(true);
    VkDeviceAddress gpuAddress = 0;
  };

  template <typename T> struct SubdivBuffer {
    SubdivBuffer() = default;
    SubdivBuffer(Buffer buffer, size_t count = 0) : buffer(std::move(buffer)), count(count) {}

    Buffer buffer{};
    size_t count = 0;

    [[nodiscard]] size_t occupied() const { return count * sizeof(T); }

    [[nodiscard]] T* end() const {
      constexpr size_t elementSize = sizeof(T);
      size_t offset = count * elementSize;

      return reinterpret_cast<T*>(buffer.mapping(offset));
    }

    void write(const T& value) {
      memcpy(end(), &value, sizeof(T));
      count++;
    }
    void write(const std::span<const T> data) {
      if (data.empty())
        return;
      memcpy(end(), data.data(), data.size() * sizeof(T));
      count += data.size();
    }

    void overwrite(const std::span<const T> data) {
      count = 0;
      write(data);
    }

    void copyTo(SubdivBuffer<T>& other) const {
      if (count == 0)
        return;
      VK_ASSERT(other.buffer.size() >= (other.count + count) * sizeof(T), "Destination buffer is too small to copy data");
      memcpy(other.end(), buffer.mapping(), count * sizeof(T));
      other.count += count;
    }

    void copyCmd(VkCommandBuffer cmdBuf, VkBuffer to) {
      if (count == 0)
        return;
      VkBufferCopy copyRegion{
          .srcOffset = 0,
          .dstOffset = 0,
          .size = count * sizeof(T),
      };
      vkCmdCopyBuffer(cmdBuf, buffer, to, 1, &copyRegion);
    }

    operator Buffer() const { return buffer; }
    Buffer* operator->() { return &buffer; }

    void destroy(const VmaAllocator& allocator) {
      buffer.destroy(allocator);
      count = 0;
    }
  };
} // namespace kt::vkh