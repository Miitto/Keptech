#include "buffer.hpp"
#include "keptech/vulkan/macros.hpp"
#include <spdlog/fmt/bundled/ranges.h>
#include <vector>
namespace kt::vkh {

  std::expected<Buffer, std::string> Buffer::create(const VkDevice device, const VmaAllocator& allocator, VkBufferCreateInfo bufInfo,
                                                    const VmaAllocationCreateInfo& allocInfo, const std::string& name) {
    VK_ASSERT(bufInfo.size > 0, "Buffer size must be greater than 0");

    bufInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VkBuffer buffer{};
    VmaAllocation alloc{};
    VmaAllocationInfo aInfo{};
    VK_MAKE(vmaCreateBuffer(allocator, &bufInfo, &allocInfo, &buffer, &alloc, &aInfo), "Failed to create allocated buffer");

    VkBufferDeviceAddressInfo addrVknfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer};
    VkDeviceAddress address = vkGetBufferDeviceAddress(device, &addrVknfo);

#if VK_LOG_LEVEL >= VK_LOG_LEVEL_TRACE
    VkMemoryPropertyFlags props{};
    vmaGetAllocationMemoryProperties(allocator, alloc, &props);
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

#ifndef NDEBUG
    if (!name.empty()) {
      vmaSetAllocationName(allocator, alloc, name.c_str());
    }
#endif

    return Buffer(buffer, alloc, aInfo, address);
  }
} // namespace kt::vkh