#include "buffer.hpp"
#if RHI_LOG_LEVEL <= SPDLOG_LEVEL_TRACE
#include <spdlog/fmt/bundled/ranges.h>
#include <vector>
#endif
#include "bufferCreateInfo.hpp"
#include "rhi.hpp"
#include "vk/vk-logger.hpp"

namespace kt::rhi {

  [[nodiscard]] bool Buffer::isMapped() const { return allocInfo.pMappedData != nullptr; }
  [[nodiscard]] uint8_t* Buffer::mapping(VkDeviceSize offset) const { return static_cast<uint8_t*>(allocInfo.pMappedData) + offset; }
  [[nodiscard]] size_t Buffer::size() const { return _size; }
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
    VK_ASSERT(info.getSize() > 0, "Buffer size must be greater than 0");

    auto& r = RHI::get();

    VkBufferCreateInfo bufInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = info.getSize(),
        .usage = static_cast<VkBufferUsageFlags>(info.getUsage().as_underlying()) | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };

    VmaAllocationCreateFlags allocFlags = 0;
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    switch (info.getType()) {
    case BufferType::Default:
      break;
    case BufferType::GpuMapped:
      allocFlags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
      break;
    case BufferType::Staging:
      allocFlags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
      memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
      break;
    case BufferType::Readback:
      allocFlags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
      memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
      break;
    }

    VmaAllocationCreateInfo allocInfo{
        .flags = allocFlags,
        .usage = memoryUsage,
    };

    VkBuffer buffer{};
    VmaAllocation alloc{};
    VmaAllocationInfo aInfo{};
    {
      auto res = vmaCreateBuffer(r.vkGetDevice(), &bufInfo, &allocInfo, &buffer, &alloc, &aInfo);
      if (res != VK_SUCCESS)
        return {res};
    }

    VkBufferDeviceAddressInfo addrVknfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .pNext = nullptr, .buffer = buffer};
    VkDeviceAddress address = vkGetBufferDeviceAddress(r.vkGetDevice(), &addrVknfo);

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
      r.vkGetDevice().setAllocationName(alloc, info.getName());
      r.vkGetDevice().setDebugName(buffer, info.getName());
    }

    return Buffer(info.getName(), buffer, bufInfo.size, alloc, aInfo, address, bufInfo.usage, allocFlags);
  }

  void Buffer::destroy() {
    if (alloc) {
      VK_ASSERT(RHI::get().vkGetDevice(), "Device is null");
      VK_TRACE("Destroying buffer {} with size {}", allocInfo.pName ? allocInfo.pName : "Unnamed", allocInfo.size);
      vmaDestroyBuffer(RHI::get().vkGetDevice(), buffer, alloc);
      buffer = nullptr;
      alloc = nullptr;
    }
  }
  Buffer::Buffer(Buffer&& other) noexcept
      : name(std::move(other.name)), buffer(other.buffer), _size(other._size), alloc(other.alloc), allocInfo(other.allocInfo),
        gpuAddress(other.gpuAddress) {
    other.buffer = VK_NULL_HANDLE;
    other._size = 0;
    other.alloc = nullptr;
    other.allocInfo = {};
    other.gpuAddress = 0;
  }
  Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
      name = std::move(other.name);
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

  const std::string& Buffer::getName() const { return name; }
  [[nodiscard]] VkBufferUsageFlags Buffer::getUsage() const { return usage; }
  [[nodiscard]] VmaAllocationCreateFlags Buffer::getAllocationFlags() const { return allocationFlags; }

  Buffer::Buffer(std::string name, VkBuffer buffer, VkDeviceSize size, VmaAllocation alloc, VmaAllocationInfo allocInfo,
                 VkDeviceAddress gpuAddress, VkBufferUsageFlags usage, VmaAllocationCreateFlags allocationFlags)
      : name(std::move(name)), buffer(buffer), _size(size), alloc(alloc), allocInfo(allocInfo), gpuAddress(gpuAddress), usage(usage),
        allocationFlags(allocationFlags) {}

} // namespace kt::rhi