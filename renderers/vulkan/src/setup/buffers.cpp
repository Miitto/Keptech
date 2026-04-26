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
  constexpr size_t INITIAL_MATERIAL_COUNT = 100;
  constexpr size_t INITIAL_OBJECT_COUNT = 100;
  constexpr size_t INITIAL_POINT_LIGHT_COUNT = 2;
  constexpr size_t INITIAL_SHADOW_MATRIX_COUNT = (INITIAL_POINT_LIGHT_COUNT * 6);
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

  std::expected<AddressedAllocatedBuffer, std::string> createMaterialBuffer(const Renderer::VulkanCore& vkcore) {
    size_t size = sizeof(Renderer::GpuMaterial) * INITIAL_MATERIAL_COUNT;

    VkBufferCreateInfo bufInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = hostWriteOrTransferFlags,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    return AddressedAllocatedBuffer::create(vkcore.device.logical, vkcore.allocator, bufInfo, allocInfo, "Material Buffer");
  }

  std::expected<AddressedAllocatedBuffer, std::string> createObjectBuffer(const Renderer::VulkanCore& vkcore) {
    size_t size = sizeof(Renderer::GpuObject) * INITIAL_OBJECT_COUNT;

    VkBufferCreateInfo bufInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = hostWriteFlags,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    return AddressedAllocatedBuffer::create(vkcore.device.logical, vkcore.allocator, bufInfo, allocInfo, "Object Buffer");
  }

  std::expected<AddressedAllocatedBuffer, std::string> createPointLightBuffer(const Renderer::VulkanCore& vkcore) {
    size_t size = sizeof(Renderer::GpuPointLight) * INITIAL_POINT_LIGHT_COUNT;

    VkBufferCreateInfo bufInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = hostWriteFlags,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    return AddressedAllocatedBuffer::create(vkcore.device.logical, vkcore.allocator, bufInfo, allocInfo, "Point Light Buffer");
  }

  std::expected<AddressedAllocatedBuffer, std::string> createShadowMatrixBuffer(const Renderer::VulkanCore& vkcore) {
    size_t size = sizeof(glm::mat4) * INITIAL_SHADOW_MATRIX_COUNT;

    VkBufferCreateInfo bufInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = hostWriteFlags,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    return AddressedAllocatedBuffer::create(vkcore.device.logical, vkcore.allocator, bufInfo, allocInfo, "Shadow Matrix Buffer");
  }

  std::expected<AddressedAllocatedBuffer, std::string> createDrawCommandBuffer(const Renderer::VulkanCore& vkcore) {
    size_t size = sizeof(VkDrawIndexedIndirectCommand) * INITIAL_OBJECT_COUNT;

    VkBufferCreateInfo bufInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = hostWriteFlags,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    return AddressedAllocatedBuffer::create(vkcore.device.logical, vkcore.allocator, bufInfo, allocInfo, "Draw Command Buffer");
  }

  std::expected<Renderer::Buffers, std::string> createBuffers(const Renderer::VulkanCore& vkcore) {
    VKH_MAKE(camera, createCameraBuffer(vkcore), "Failed to create camera uniform buffer.");
    VKH_MAKE(ssaoKernel, createSsaoKernelBuffer(vkcore), "Failed to create SSAO kernel buffer.");
    VKH_MAKE(vertexBuffer, createVertexBuffer(vkcore), "Failed to create vertex buffer.");
    VKH_MAKE(indexBuffer, createIndexBuffer(vkcore), "Failed to create index buffer.");
    VKH_MAKE(materialBuffer, createMaterialBuffer(vkcore), "Failed to create material buffer.");

    std::array<Renderer::PerFrameBuffers, MAX_FRAMES_IN_FLIGHT> perFrameBuffers;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      VKH_MAKE(objectBuffer, createObjectBuffer(vkcore), "Failed to create object buffer.");
      VKH_MAKE(pointLightBuffer, createPointLightBuffer(vkcore), "Failed to create point light buffer.");
      VKH_MAKE(shadowMatrixBuffer, createShadowMatrixBuffer(vkcore), "Failed to create shadow matrix buffer.");
      VKH_MAKE(drawCommandBuffer, createDrawCommandBuffer(vkcore), "Failed to create draw command buffer.");

      perFrameBuffers[i] = Renderer::PerFrameBuffers{
          .objects = {.buffer = objectBuffer},
          .pointLights = {.buffer = pointLightBuffer},
          .shadowMatrices = {.buffer = shadowMatrixBuffer},
          .drawCommands = {.buffer = drawCommandBuffer},
      };
    }

    return Renderer::Buffers{
        .camera = camera,
        .ssaoKernel = ssaoKernel,
        .vertices = {.buffer = vertexBuffer},
        .indices = {.buffer = indexBuffer},
        .materials = {.buffer = materialBuffer},
        .perFrame = perFrameBuffers,
    };
  }
} // namespace kt::vkh::setup
