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

  template <typename T>
  std::expected<AddressedAllocatedBuffer, std::string> createBuffer(const Renderer::VulkanCore& vkcore, const size_t elementCount,
                                                                    const std::string name, const VkBufferUsageFlags usage = 0,
                                                                    const bool allowTransfer = false) {
    size_t size = sizeof(T) * elementCount;

    VkBufferCreateInfo bufInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                 (allowTransfer ? (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT) : 0),
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = allowTransfer ? hostWriteOrTransferFlags : hostWriteFlags,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    return AddressedAllocatedBuffer::create(vkcore.device.logical, vkcore.allocator, bufInfo, allocInfo, name);
  }

  std::expected<Renderer::Buffers, std::string> createBuffers(const Renderer::VulkanCore& vkcore) {
    VKH_MAKE(camera, createCameraBuffer(vkcore), "Failed to create camera uniform buffer.");
    VKH_MAKE(ssaoKernel,
             createBuffer<glm::vec4>(vkcore, constants::SSAO_KERNEL_SIZE, "SSAO Kernel Buffer", VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true),
             "Failed to create SSAO kernel buffer.");
    VKH_MAKE(vertexPosBuffer,
             createBuffer<glm::vec3>(vkcore, INITIAL_VERTEX_COUNT, "Vertex Position Buffer", VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true),
             "Failed to create vertex position buffer.");
    VKH_MAKE(vertexAttribBuffer,
             createBuffer<VertexAttribs>(vkcore, INITIAL_VERTEX_COUNT, "Vertex Attrib Buffer", VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true),
             "Failed to create vertex buffer.");
    VKH_MAKE(indexBuffer, createBuffer<uint32_t>(vkcore, INITIAL_INDEX_COUNT, "Index Buffer", VK_BUFFER_USAGE_INDEX_BUFFER_BIT, true),
             "Failed to create index buffer.");
    VKH_MAKE(materialBuffer, createBuffer<Renderer::GpuMaterial>(vkcore, INITIAL_MATERIAL_COUNT, "Material Buffer", 0, true),
             "Failed to create material buffer.");

    std::array<Renderer::PerFrameBuffers, MAX_FRAMES_IN_FLIGHT> perFrameBuffers;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      VKH_MAKE(objectBuffer, createBuffer<Renderer::GpuObject>(vkcore, INITIAL_OBJECT_COUNT, "Object Buffer"),
               "Failed to create object buffer.");
      VKH_MAKE(pointLightBuffer, createBuffer<Renderer::GpuPointLight>(vkcore, INITIAL_POINT_LIGHT_COUNT, "Point Light Buffer"),
               "Failed to create point light buffer.");
      VKH_MAKE(shadowMatrixBuffer, createBuffer<glm::mat4>(vkcore, INITIAL_SHADOW_MATRIX_COUNT, "Shadow Matrix Buffer"),
               "Failed to create shadow matrix buffer.");
      VKH_MAKE(drawCommandBuffer,
               createBuffer<VkDrawIndexedIndirectCommand>(vkcore, INITIAL_OBJECT_COUNT, "Draw Command Buffer",
                                                          VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
               "Failed to create draw command buffer.");
      VKH_MAKE(shadowDrawCommandBuffer,
               createBuffer<VkDrawIndexedIndirectCommand>(vkcore, INITIAL_OBJECT_COUNT * INITIAL_POINT_LIGHT_COUNT,
                                                          "Shadow Draw Command Buffer", VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
               "Failed to create shadow draw command buffer.");

      perFrameBuffers[i] = Renderer::PerFrameBuffers{
          .objects = {.buffer = objectBuffer},
          .pointLights = {.buffer = pointLightBuffer},
          .shadowMatrices = {.buffer = shadowMatrixBuffer},
          .drawCommands = {.buffer = drawCommandBuffer},
          .shadowDrawCommands = {.buffer = shadowDrawCommandBuffer},
      };
    }

    return Renderer::Buffers{
        .camera = camera,
        .ssaoKernel = ssaoKernel,
        .vertexPositions = {.buffer = vertexPosBuffer},
        .vertexAttribs = {.buffer = vertexAttribBuffer},
        .indices = {.buffer = indexBuffer},
        .materials = {.buffer = materialBuffer},
        .perFrame = perFrameBuffers,
    };
  }
} // namespace kt::vkh::setup
