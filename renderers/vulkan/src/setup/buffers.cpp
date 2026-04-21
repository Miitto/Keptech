#include "setup.hpp"

#include "keptech/components/camera.hpp"
#include "keptech/maths/maths.hpp"
#include "keptech/vulkan/constants.hpp"
#include "macros.hpp"

namespace {
  constexpr VmaAllocationCreateFlags hostWriteFlags =
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
  constexpr VmaAllocationCreateFlags hostWriteOrTransferFlags =
      hostWriteFlags | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;

  constexpr size_t INITIAL_VERTEX_COUNT = 1000;
  constexpr size_t INITIAL_INDEX_COUNT = 1000;
} // namespace

namespace kt::vkh::setup {
  std::expected<AddressedAllocatedBuffer, std::string> createCameraBuffer(const Renderer::VulkanCore& vkcore) {
    size_t size = sizeof(components::Camera::Uniforms);
    for (size_t i = 1; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      size = maths::roundToAlignment(size, limits::minUniformBufferOffsetAlignment);
      size += sizeof(components::Camera::Uniforms);
    }

    VkBufferCreateInfo bufInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = hostWriteFlags,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    return AddressedAllocatedBuffer::create(vkcore.device.logical, vkcore.allocator, bufInfo, allocInfo, "Camera Uniform Buffer");
  }

  std::expected<AddressedAllocatedBuffer, std::string> createSsaoKernelBuffer(const Renderer::VulkanCore& vkcore) {
    size_t size = sizeof(glm::vec4) * constants::SSAO_KERNEL_SIZE;

    VkBufferCreateInfo bufInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = hostWriteOrTransferFlags,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    return AddressedAllocatedBuffer::create(vkcore.device.logical, vkcore.allocator, bufInfo, allocInfo, "SSAO Kernel Buffer");
  }

  std::expected<AddressedAllocatedBuffer, std::string> createVertexBuffer(const Renderer::VulkanCore& vkcore) {
    size_t size = sizeof(Vertex) * INITIAL_VERTEX_COUNT;

    VkBufferCreateInfo bufInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = hostWriteOrTransferFlags,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    return AddressedAllocatedBuffer::create(vkcore.device.logical, vkcore.allocator, bufInfo, allocInfo, "Vertex Buffer");
  }

  std::expected<AddressedAllocatedBuffer, std::string> createIndexBuffer(const Renderer::VulkanCore& vkcore) {
    size_t size = sizeof(uint32_t) * INITIAL_INDEX_COUNT;

    VkBufferCreateInfo bufInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = hostWriteOrTransferFlags,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    return AddressedAllocatedBuffer::create(vkcore.device.logical, vkcore.allocator, bufInfo, allocInfo, "Index Buffer");
  }

  std::expected<Renderer::Buffers, std::string> createBuffers(const Renderer::VulkanCore& vkcore) {
    VKH_MAKE(camera, createCameraBuffer(vkcore), "Failed to create camera uniform buffer.");
    VKH_MAKE(ssaoKernel, createSsaoKernelBuffer(vkcore), "Failed to create SSAO kernel buffer.");
    VKH_MAKE(vertexBuffer, createVertexBuffer(vkcore), "Failed to create vertex buffer.");
    VKH_MAKE(indexBuffer, createIndexBuffer(vkcore), "Failed to create index buffer.");

    return Renderer::Buffers{
        .camera = camera,
        .ssaoKernel = ssaoKernel,
        .vertices = {.buffer = vertexBuffer},
        .indices = {.buffer = indexBuffer},
    };
  }
} // namespace kt::vkh::setup
