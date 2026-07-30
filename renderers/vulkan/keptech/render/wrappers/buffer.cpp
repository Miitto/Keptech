#include "buffer.hpp"
#if RENDERER_LOG_LEVEL <= SPDLOG_LEVEL_TRACE
#include <spdlog/fmt/bundled/ranges.h>
#include <vector>
#endif
#include "bufferCreateInfo.hpp"
#include "keptech/render/vk-logger.hpp"
#include "renderer.hpp"

namespace kt::rdr {

  [[nodiscard]] bool Buffer::isMapped() const { return allocInfo.pMappedData != nullptr; }
  [[nodiscard]] uint8_t* Buffer::mapping(VkDeviceSize offset) const { return static_cast<uint8_t*>(allocInfo.pMappedData) + offset; }
  [[nodiscard]] size_t Buffer::size() const { return allocInfo.size; }
  [[nodiscard]] VkDeviceAddress Buffer::address() const { return gpuAddress; }
  Buffer::operator VkBuffer() const { return buffer; }

  void Buffer::write(void* data, size_t size, VkDeviceSize offset) const {
    VK_ASSERT(isMapped(), "Buffer is not mapped");
    VK_ASSERT(offset + size <= this->size(), "Write exceeds buffer size");
    memcpy(mapping(offset), data, size);
  }
  void Buffer::copyTo(const Buffer& other, VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) const {
    VK_ASSERT(isMapped() && other.isMapped(), "Both buffers must be mapped to copy data");
    VK_ASSERT(other.size() >= size, "Destination buffer is too small to copy data");
    memcpy(other.mapping(dstOffset), mapping(srcOffset), size);
  }
  void Buffer::copyCmd(VkCommandBuffer cmdBuf, const Buffer& other, VkDeviceSize size, VkDeviceSize srcOffset,
                       VkDeviceSize dstOffset) const {
    VK_ASSERT(other.size() >= size, "Destination buffer is too small to copy data");
    VkBufferCopy copyRegion{
        .srcOffset = srcOffset,
        .dstOffset = dstOffset,
        .size = size,
    };
    vkCmdCopyBuffer(cmdBuf, static_cast<VkBuffer>(*this), static_cast<VkBuffer>(other), 1, &copyRegion);
  }

  kt::Result<Buffer, VkResult, VK_SUCCESS> Buffer::create(const BufferCreateInfo& info) {
    VK_ASSERT(info.getBufferInfo().size > 0, "Buffer size must be greater than 0");

    auto& r = Renderer::get();

    auto bufInfo = info.getBufferInfo();
    bufInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VkBuffer buffer{};
    VmaAllocation alloc{};
    VmaAllocationInfo aInfo{};
    {
      auto res = vmaCreateBuffer(r.getDevice(), &bufInfo, &info.getAllocInfo(), &buffer, &alloc, &aInfo);
      if (res != VK_SUCCESS)
        return {res};
    }

    VkBufferDeviceAddressInfo addrVknfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .pNext = nullptr, .buffer = buffer};
    VkDeviceAddress address = vkGetBufferDeviceAddress(r.getDevice(), &addrVknfo);

#if RENDERER_LOG_LEVEL <= SPDLOG_LEVEL_TRACE
    VkMemoryPropertyFlags props{};
    vmaGetAllocationMemoryProperties(r.getDevice(), alloc, &props);
    bool isHostVisible = (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
    bool deviceLocal = (props & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
    bool mapped = aInfo.pMappedData != nullptr;

    std::vector<std::string> flags;
    if (isHostVisible)
      flags.emplace_back("Host Visible");
    if (deviceLocal)
      flags.emplace_back("Device Local");
    if (mapped)
      flags.emplace_back("Mapped");

    VK_TRACE("Created buffer [{}] with size {} bytes. {}", name, aInfo.size, fmt::join(flags, ", "));
#endif

    if (info.getName()) {
      r->setAllocationName(alloc, info.getName());
      r->setDebugName(buffer, info.getName());
    }

    return Buffer(buffer, bufInfo.size, alloc, aInfo, address, bufInfo.usage, info.getAllocInfo().flags);
  }

  void Buffer::destroy() {
    if (alloc) {
      VK_ASSERT(Renderer::get().getDevice(), "Device is null");
      VK_TRACE("Destroying buffer {} with size {}", allocInfo.pName ? allocInfo.pName : "Unnamed", allocInfo.size);
      vmaDestroyBuffer(Renderer::get().getDevice(), buffer, alloc);
      buffer = nullptr;
      alloc = nullptr;
    }
  }
  Buffer::Buffer(Buffer&& other) noexcept
      : buffer(other.buffer), _size(other._size), alloc(other.alloc), allocInfo(other.allocInfo), gpuAddress(other.gpuAddress) {
    other.buffer = VK_NULL_HANDLE;
    other._size = 0;
    other.alloc = nullptr;
    other.allocInfo = {};
    other.gpuAddress = 0;
  }
  Buffer& Buffer::operator=(Buffer&& other) noexcept {
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
  [[nodiscard]] VkBufferUsageFlags Buffer::getUsage() const { return usage; }
  [[nodiscard]] VmaAllocationCreateFlags Buffer::getAllocationFlags() const { return allocationFlags; }
  Buffer::Buffer(VkBuffer buffer, VkDeviceSize size, VmaAllocation alloc, VmaAllocationInfo allocInfo, VkDeviceAddress gpuAddress,
                 VkBufferUsageFlags usage, VmaAllocationCreateFlags allocationFlags)
      : buffer(buffer), _size(size), alloc(alloc), allocInfo(allocInfo), gpuAddress(gpuAddress), usage(usage),
        allocationFlags(allocationFlags) {}

} // namespace kt::rdr