#include "buffer.hpp"
#if RENDERER_LOG_LEVEL <= SPDLOG_LEVEL_TRACE
#include <spdlog/fmt/bundled/ranges.h>
#include <vector>
#endif
#include "bufferCreateInfo.hpp"
#include "renderer.hpp"

namespace kt::vkh {

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

    return Buffer(buffer, bufInfo.size, alloc, aInfo, address);
  }

  void Buffer::destroy() {
    VK_ASSERT(Renderer::isInit(), "Device is null");
    if (alloc) {
      VK_TRACE("Destroying buffer {} with size {}", allocInfo.pName ? allocInfo.pName : "Unnamed", allocInfo.size);
      vmaDestroyBuffer(Renderer::get().getDevice(), buffer, alloc);
      buffer = nullptr;
      alloc = nullptr;
    }
  }
} // namespace kt::vkh