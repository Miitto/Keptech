#include "setup.hpp"

#include "keptech/components/camera.hpp"
#include "macros.hpp"

namespace kt::vkh::setup {
  std::expected<Renderer::Buffers, std::string> createBuffers(const Renderer::VulkanCore& vkcore) {
    VkBufferCreateInfo bufInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(components::Camera::Uniforms) * MAX_FRAMES_IN_FLIGHT,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT |
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    VKH_MAKE(camera, AddressedAllocatedBuffer::create(vkcore.device.logical, vkcore.allocator, bufInfo, allocInfo, "Camera Uniform Buffer"),
             "Failed to create camera uniform buffer.");

    return Renderer::Buffers{
        .camera = camera,
    };
  }
} // namespace kt::vkh::setup
