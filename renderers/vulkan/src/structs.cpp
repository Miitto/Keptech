#include <keptech/vulkan/structs.hpp>

#include "macros.hpp"

namespace keptech::vkh {
  std::expected<AllocatedBuffer, std::string>
  AllocatedBuffer::create(vma::Allocator& allocator,
                          const vk::BufferCreateInfo& bufInfo,
                          const vma::AllocationCreateInfo& allocInfo) {
    vma::AllocationInfo aInfo = {};
    VMA_MAKE(buffer, allocator.createBuffer(bufInfo, allocInfo, aInfo),
             "Failed to create allocated buffer");

    return AllocatedBuffer{
        .buffer = buffer.first,
        .alloc = buffer.second,
        .allocInfo = aInfo,
    };
  }
} // namespace keptech::vkh
