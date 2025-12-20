#include "keptech/vulkan/mesh.hpp"

#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"

namespace keptech::vkh {
  std::expected<std::pair<Mesh, OnGoingCmdTransfer>, std::string>
  Mesh::fromData(const vk::raii::Device& device, vma::Allocator& allocator,
                 const CommandPool& transferPool,
                 std::span<const Vertex> vertices,
                 std::span<const uint32_t> indices,
                 std::vector<Mesh::Submesh> submeshes) {

    vk::DeviceSize verticesSize = sizeof(Vertex) * vertices.size();
    vk::DeviceSize indicesSize = sizeof(uint32_t) * indices.size();

    vk::DeviceSize totalSize = verticesSize + indicesSize;

    vk::BufferCreateInfo stagingBufferInfo{
        .size = totalSize,
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
        .sharingMode = vk::SharingMode::eExclusive,
    };

    vma::AllocationCreateInfo stagingAllocInfo{
        .flags = vma::AllocationCreateFlagBits::eMapped,
        .usage = vma::MemoryUsage::eCpuToGpu,
    };

    VK_MAKE(fence, device.createFence({}),
            "Failed to create fence for mesh upload");

    VKH_MAKE(
        stagingBuf,
        AllocatedBuffer::create(allocator, stagingBufferInfo, stagingAllocInfo),
        "Failed to create staging buffer");

    vk::BufferCreateInfo vertexBufferInfo{
        .size = verticesSize,
        .usage = vk::BufferUsageFlagBits::eVertexBuffer |
                 vk::BufferUsageFlagBits::eTransferDst |
                 vk::BufferUsageFlagBits::eShaderDeviceAddress,
        .sharingMode = vk::SharingMode::eExclusive,
    };

    vma::AllocationCreateInfo vertexAllocInfo{
        .usage = vma::MemoryUsage::eGpuOnly,
    };

    auto vertexBufferRes = AddressedAllocatedBuffer::create(
        device, allocator, vertexBufferInfo, vertexAllocInfo);
    if (!vertexBufferRes) {
      stagingBuf.destroy(allocator);
      return std::unexpected(vertexBufferRes.error());
    }
    auto vertexBuffer = *vertexBufferRes;

    std::optional<AllocatedBuffer> indexBuffer = std::nullopt;
    if (!indices.empty()) {
      vk::BufferCreateInfo indexBufferInfo{
          .size = indicesSize,
          .usage = vk::BufferUsageFlagBits::eIndexBuffer |
                   vk::BufferUsageFlagBits::eTransferDst,
          .sharingMode = vk::SharingMode::eExclusive,
      };
      vma::AllocationCreateInfo indexAllocInfo{
          .usage = vma::MemoryUsage::eGpuOnly,
      };
      auto indexBufferRes =
          AllocatedBuffer::create(allocator, indexBufferInfo, indexAllocInfo);
      if (!indexBufferRes) {
        stagingBuf.destroy(allocator);
        vertexBuffer.destroy(allocator);
        return std::unexpected(indexBufferRes.error());
      }
      indexBuffer = *indexBufferRes;
    }

    vk::CommandBufferAllocateInfo cmdBufAllocInfo{
        .commandPool = *transferPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };

    auto cmdBuffersRes = device.allocateCommandBuffers(cmdBufAllocInfo);
    if (cmdBuffersRes.result != vk::Result::eSuccess) {
      stagingBuf.destroy(allocator);
      vertexBuffer.destroy(allocator);
      if (indexBuffer.has_value())
        indexBuffer->destroy(allocator);
      return std::unexpected("Failed to allocate command buffers");
    }
    vk::raii::CommandBuffer cmdBuffer = std::move(cmdBuffersRes.value[0]);

    memcpy(stagingBuf.mapping(), vertices.data(),
           static_cast<size_t>(verticesSize));

    cmdBuffer.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });

    cmdBuffer.copyBuffer(stagingBuf.buffer, vertexBuffer.buffer,
                         vk::BufferCopy{
                             .size = verticesSize,
                         });

    if (indexBuffer.has_value()) {
      memcpy(stagingBuf.mapping(verticesSize), indices.data(),
             static_cast<size_t>(indicesSize));
      cmdBuffer.copyBuffer(stagingBuf.buffer, indexBuffer->buffer,
                           vk::BufferCopy{
                               .srcOffset = verticesSize,
                               .size = indicesSize,
                           });
    }

    cmdBuffer.end();

    vk::SubmitInfo submitInfo{
        .commandBufferCount = 1,
        .pCommandBuffers = &*cmdBuffer,
    };

    transferPool.queue.queue->submit({submitInfo}, fence);

    if (submeshes.empty()) {
      uint32_t indexCount = indices.empty()
                                ? static_cast<uint32_t>(vertices.size())
                                : static_cast<uint32_t>(indices.size());
      submeshes.push_back(Mesh::Submesh{
          .indexCount = indexCount,
          .indexOffset = 0,
      });
    }

    return std::make_pair(
        Mesh{
            .vertexBuffer = vertexBuffer,
            .indexBuffer = indexBuffer,
            .submeshes = std::move(submeshes),
        },
        OnGoingCmdTransfer{.cmdBuffer = std::move(cmdBuffer),
                           .buffer = stagingBuf,
                           .fence = std::move(fence)});
  }
} // namespace keptech::vkh
