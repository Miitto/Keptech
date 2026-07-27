#pragma once

#include "keptech/core/result.hpp"
#include "keptech/render/vk-logger.hpp"
#include <Volk/volk.h>
#include <span>
#include <utility>
#include <vma/vk_mem_alloc.h>

namespace kt::vkh {
  class BufferCreateInfo;

  /// @brief A RAII wrapper around a Vulkan buffer and its associated memory allocation. When the Buffer is destroyed, the underlying Vulkan
  /// resources are automatically cleaned up.
  class Buffer {
  public:
    Buffer() = default;
    [[nodiscard]] bool isMapped() const { return allocInfo.pMappedData != nullptr; }
    [[nodiscard]] uint8_t* mapping(VkDeviceSize offset = 0) const { return static_cast<uint8_t*>(allocInfo.pMappedData) + offset; }
    [[nodiscard]] size_t size() const { return allocInfo.size; }
    [[nodiscard]] VkDeviceAddress address() const { return gpuAddress; }

    operator VkBuffer() const { return buffer; }

    /// @brief Writes data to the buffer at the specified offset. The buffer must be mapped before calling this method.
    /// @param data Pointer to the data to write.
    /// @param size Size of the data to write in bytes.
    /// @param offset Offset in the buffer where the data should be written. Defaults to 0.
    void write(void* data, size_t size, VkDeviceSize offset = 0) const {
      VK_ASSERT(isMapped(), "Buffer is not mapped");
      VK_ASSERT(offset + size <= this->size(), "Write exceeds buffer size");
      memcpy(mapping(offset), data, size);
    }
    /// @brief Copies data from this buffer to another buffer. Both buffers must be mapped before calling this method.
    /// @param other The destination buffer to copy data to.
    /// @param size Size of the data to copy in bytes.
    /// @param srcOffset Offset in this buffer from where the data should be copied. Defaults to 0.
    /// @param dstOffset Offset in the destination buffer where the data should be written. Defaults to 0.
    void copyTo(const Buffer& other, VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0) const {
      VK_ASSERT(isMapped() && other.isMapped(), "Both buffers must be mapped to copy data");
      VK_ASSERT(other.size() >= size, "Destination buffer is too small to copy data");
      memcpy(other.mapping(dstOffset), mapping(srcOffset), size);
    }
    /// @brief Records a command to copy data from this buffer to another buffer. This method does not require the buffers to be mapped.
    /// @param cmdBuf The command buffer to record the copy command into.
    /// @param other The destination buffer to copy data to.
    /// @param size Size of the data to copy in bytes.
    /// @param srcOffset Offset in this buffer from where the data should be copied. Defaults to 0.
    /// @param dstOffset Offset in the destination buffer where the data should be written. Defaults to 0.
    void copyCmd(VkCommandBuffer cmdBuf, const Buffer& other, VkDeviceSize size, VkDeviceSize srcOffset = 0,
                 VkDeviceSize dstOffset = 0) const {
      VK_ASSERT(other.size() >= size, "Destination buffer is too small to copy data");
      VkBufferCopy copyRegion{
          .srcOffset = srcOffset,
          .dstOffset = dstOffset,
          .size = size,
      };
      vkCmdCopyBuffer(cmdBuf, static_cast<VkBuffer>(*this), static_cast<VkBuffer>(other), 1, &copyRegion);
    }

    static kt::Result<Buffer, VkResult, VK_SUCCESS> create(const BufferCreateInfo& info);

    void destroy();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept
        : buffer(other.buffer), _size(other._size), alloc(other.alloc), allocInfo(other.allocInfo), gpuAddress(other.gpuAddress) {
      other.buffer = VK_NULL_HANDLE;
      other._size = 0;
      other.alloc = nullptr;
      other.allocInfo = {};
      other.gpuAddress = 0;
    }
    Buffer& operator=(Buffer&& other) noexcept {
      if (this != &other) {
        buffer = other.buffer;
        _size = other._size;
        alloc = other.alloc;
        allocInfo = other.allocInfo;
        gpuAddress = other.gpuAddress;

        other.buffer = VK_NULL_HANDLE;
        other._size = 0;
        other.alloc = nullptr;
        other.allocInfo = {};
        other.gpuAddress = 0;
      }
      return *this;
    }
    ~Buffer() { destroy(); }

  private:
    Buffer(VkBuffer buffer, VkDeviceSize size, VmaAllocation alloc, VmaAllocationInfo allocInfo, VkDeviceAddress gpuAddress)
        : buffer(buffer), _size(size), alloc(alloc), allocInfo(allocInfo), gpuAddress(gpuAddress) {}

    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceSize _size = 0;
    VmaAllocation alloc = nullptr;
    VmaAllocationInfo allocInfo = {};
    VkDeviceAddress gpuAddress = 0;
  };

  template <typename T> class SubdivBuffer {
  public:
    SubdivBuffer() = default;
    SubdivBuffer(const SubdivBuffer& other) = delete;
    SubdivBuffer& operator=(const SubdivBuffer& other) = delete;
    SubdivBuffer(Buffer&& buffer, size_t count = 0) : buffer(std::move(buffer)), _count(count) {}
    SubdivBuffer(SubdivBuffer&& other) noexcept : buffer(std::move(other.buffer)), _count(other._count) { other._count = 0; }
    SubdivBuffer& operator=(SubdivBuffer&& other) noexcept {
      if (this != &other) {
        buffer = std::move(other.buffer);
        _count = other._count;
        other._count = 0;
      }
      return *this;
    }
    ~SubdivBuffer() = default;

    /// @brief Returns the number of elements currently stored in the buffer.
    [[nodiscard]] size_t count() const { return _count; }
    /// @brief Returns the total size of the buffer in bytes.
    [[nodiscard]] size_t size() const { return buffer.size(); }
    /// @brief Returns the number of bytes currently occupied by the elements in the buffer.
    [[nodiscard]] size_t occupied() const { return _count * sizeof(T); }

    /// @brief Returns a pointer to the start of the buffer's data. Will be nullptr if the buffer is not mapped. This is equivalent to
    /// calling `.mapping()` on the underlying buffer.
    [[nodiscard]] T* begin() const { return reinterpret_cast<T*>(buffer.mapping()); }
    /// @brief Returns a pointer to the end of the buffer's data, which is the location where the next element would be written. Will be
    /// nullptr if the buffer is not mapped. This is equivalent to calling `.mapping(offset)` on the underlying buffer, where `offset` is
    /// the number of bytes returned by `.occupied()`.
    [[nodiscard]] T* end() const {
      constexpr size_t elementSize = sizeof(T);
      size_t offset = _count * elementSize;

      return reinterpret_cast<T*>(buffer.mapping(offset));
    }

    /// @brief Resets the buffer's element count to zero, effectively clearing the buffer. This does not deallocate or modify the underlying
    /// buffer's memory.
    void clear() { _count = 0; }

    /// @brief Writes a single element to the buffer at the current end position. The buffer must be mapped before calling this method.
    /// @param value The element to write to the buffer.
    void write(const T& value) {
      memcpy(end(), &value, sizeof(T));
      _count++;
    }
    /// @brief Writes multiple elements to the buffer at the current end position. The buffer must be mapped before calling this method.
    /// @param data A span containing the elements to write to the buffer.
    void write(const std::span<const T> data) {
      if (data.empty())
        return;
      memcpy(end(), data.data(), data.size() * sizeof(T));
      _count += data.size();
    }

    /// @brief Overwrites the buffer with the provided data, resetting the count to zero before writing. The buffer must be mapped before
    /// calling this method.
    /// @param data A span containing the elements to write to the buffer.
    void overwrite(const std::span<const T> data) {
      _count = 0;
      write(data);
    }

    /// @brief Copies the contents of this buffer to the end of another buffer. The buffer must be mapped before calling this method.
    /// @param other The buffer to copy to.
    void copyTo(SubdivBuffer<T>& other) const {
      if (_count == 0)
        return;
      buffer.copyTo(other.buffer, occupied(), 0, other.occupied());
      other._count += _count;
    }

    /// @brief Copies the contents of this buffer to the end of another buffer using a command buffer. The buffer must be mapped before
    /// calling this method.
    /// @param cmdBuf The command buffer to use for the copy operation.
    /// @param other The buffer to copy to.
    void copyCmd(VkCommandBuffer cmdBuf, SubdivBuffer<T>& other) const {
      if (_count == 0)
        return;
      buffer.copyCmd(cmdBuf, other.buffer, occupied(), 0, other.occupied());
      other._count += _count;
    }

    Buffer& operator*() { return buffer; }
    Buffer* operator->() { return &buffer; }
    const Buffer& operator*() const { return buffer; }
    const Buffer* operator->() const { return &buffer; }

    operator VkBuffer() const { return buffer; }

  private:
    Buffer buffer{};
    size_t _count = 0;
  };

} // namespace kt::vkh