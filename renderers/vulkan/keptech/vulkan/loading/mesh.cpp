#include "mesh.hpp"

#include "keptech/rendering/gltf/data.hpp"
#include "macros.hpp"
#include "profile.hpp"
#include "vk-logger.hpp"
#include <glm/vec3.hpp>

namespace kt::vkh::loading {
  namespace {
    template <typename T>
    std::expected<std::optional<Buffer>, std::string> realloc(SubdivBuffer<T>& buf, const VkDevice& device, const VmaAllocator& allocator,
                                                              size_t newSize, VkBufferUsageFlags usage, const std::string& name) {
      std::optional<Buffer> reallocatedBuffer;
      if (newSize > buf.buffer.size()) {
        VK_DEBUG("Current {} buffer size {} is too small for {}, creating new buffer", name, buf.buffer.size(), newSize);
        VkBufferCreateInfo bufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = newSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        constexpr VmaAllocationCreateInfo allocInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                     VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        };
        VKH_MAKE(rbuffer, Buffer::create(device, allocator, bufferCreateInfo, allocInfo, name), "Failed to create buffer for mesh upload");
        SubdivBuffer<T> newBuffer(std::move(rbuffer));
        buf.copyTo(newBuffer);
        reallocatedBuffer = std::move(buf.buffer);
        buf = std::move(newBuffer);
      }
      return {reallocatedBuffer};
    }
  } // namespace

  std::expected<MaybeReallocResult<uint32_t>, std::string> uploadVertices(const gltf::MeshData& data, const VkDevice& device,
                                                                          const VmaAllocator& allocator,
                                                                          SubdivBuffer<glm::vec3>& positionBuffer,
                                                                          SubdivBuffer<VertexAttribs>& attribBuffer) {
    KT_PROFILE_FUNCTION

    size_t vertexCount = data.positions.size();

    size_t newPositionsSize = vertexCount * sizeof(glm::vec3);
    size_t newVertexAttribsSize = vertexCount * sizeof(VertexAttribs);

    size_t totalPositionsSize = positionBuffer.occupied() + newPositionsSize;
    size_t totalVertexAttribsSize = attribBuffer.occupied() + newVertexAttribsSize;

    VKH_MAKE(
        oldPosBuf,
        realloc(positionBuffer, device, allocator, totalPositionsSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, "Mesh vertex position buffer"),
        "Failed to reallocate vertex position buffer");

    VKH_MAKE(
        oldAttribBuf,
        realloc(attribBuffer, device, allocator, totalVertexAttribsSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, "Mesh vertex attrib buffer"),
        "Failed to reallocate vertex attrib buffer");

    auto count = static_cast<uint32_t>(positionBuffer.count);

    positionBuffer.write(data.positions);
    attribBuffer.write(data.vertexAttribs);

    std::vector<Buffer> reallocatedBuffers;
    if (oldPosBuf) {
      reallocatedBuffers.push_back(std::move(*oldPosBuf));
    }
    if (oldAttribBuf) {
      reallocatedBuffers.push_back(std::move(*oldAttribBuf));
    }

    return {{.result = count, .reallocatedBuffers = reallocatedBuffers}};
  }

  std::expected<MaybeReallocResult<uint32_t>, std::string>
  uploadIndices(const gltf::MeshData& data, const VkDevice& device, const VmaAllocator& allocator, SubdivBuffer<uint32_t>& indexBuffer) {
    KT_PROFILE_FUNCTION

    size_t indexCount = data.indices.size();
    size_t newIndicesSize = indexCount * sizeof(uint32_t);
    size_t totalIndicesSize = indexBuffer.occupied() + newIndicesSize;

    VKH_MAKE(old, realloc(indexBuffer, device, allocator, totalIndicesSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, "Mesh index buffer"),
             "Failed to reallocate index buffer");

    auto count = static_cast<uint32_t>(indexBuffer.count);

    indexBuffer.write(data.indices);

    std::vector<Buffer> reallocatedBuffers;
    if (old) {
      reallocatedBuffers.push_back(std::move(*old));
    }

    return {{.result = count, .reallocatedBuffers = reallocatedBuffers}};
  }

  std::expected<MaybeReallocResult<MeshletBufferOffsets>, std::string>
  uploadMeshlets(const gltf::MeshData& data, const VkDevice& device, const VmaAllocator& allocator, SubdivBuffer<Meshlet>& meshletBuffer,
                 SubdivBuffer<uint32_t>& meshletVertexBuffer, SubdivBuffer<uint32_t>& meshletTriangleBuffer) {
    KT_PROFILE_FUNCTION

    size_t newMeshletsSize = data.meshlets.size() * sizeof(Meshlet);
    size_t newMeshletVerticesSize = data.meshletVertices.size() * sizeof(uint32_t);
    size_t newMeshletTrianglesSize = data.meshletTriangles.size() * sizeof(uint32_t);

    size_t totalMeshletsSize = meshletBuffer.occupied() + newMeshletsSize;
    size_t totalMeshletVerticesSize = meshletVertexBuffer.occupied() + newMeshletVerticesSize;
    size_t totalMeshletTrianglesSize = meshletTriangleBuffer.occupied() + newMeshletTrianglesSize;

    VKH_MAKE(oldMeshletBuf,
             realloc(meshletBuffer, device, allocator, totalMeshletsSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "Mesh meshlet buffer"),
             "Failed to reallocate meshlet buffer");
    VKH_MAKE(oldMeshletVertexBuf,
             realloc(meshletVertexBuffer, device, allocator, totalMeshletVerticesSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     "Mesh meshlet vertex buffer"),
             "Failed to reallocate meshlet vertex buffer");
    VKH_MAKE(oldMeshletTriangleBuf,
             realloc(meshletTriangleBuffer, device, allocator, totalMeshletTrianglesSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     "Mesh meshlet triangle buffer"),
             "Failed to reallocate meshlet triangle buffer");

    MeshletBufferOffsets offsets{
        .meshlet = static_cast<uint32_t>(meshletBuffer.count),
        .vertex = static_cast<uint32_t>(meshletVertexBuffer.count),
        .triangle = static_cast<uint32_t>(meshletTriangleBuffer.count),
    };

    meshletBuffer.write(data.meshlets);
    meshletVertexBuffer.write(data.meshletVertices);
    meshletTriangleBuffer.write(data.meshletTriangles);

    std::vector<Buffer> reallocatedBuffers;
    if (oldMeshletBuf) {
      reallocatedBuffers.push_back(std::move(*oldMeshletBuf));
    }
    if (oldMeshletVertexBuf) {
      reallocatedBuffers.push_back(std::move(*oldMeshletVertexBuf));
    }
    if (oldMeshletTriangleBuf) {
      reallocatedBuffers.push_back(std::move(*oldMeshletTriangleBuf));
    }

    return {{.result = offsets, .reallocatedBuffers = reallocatedBuffers}};
  }

  std::expected<std::vector<Buffer>, std::string>
  ensureBuffersAreLargeEnough(const VkDevice& device, const VmaAllocator& allocator, const std::vector<gltf::MeshData>& meshes,
                              SubdivBuffer<glm::vec3>& positionBuffer, SubdivBuffer<VertexAttribs>& attribBuffer,
                              SubdivBuffer<uint32_t>& indexBuffer, SubdivBuffer<Meshlet>& meshletBuffer,
                              SubdivBuffer<uint32_t>& meshletVertexBuffer, SubdivBuffer<uint32_t>& meshletTriangleBuffer) {
    size_t newVertexCount = 0;
    size_t newIndexCount = 0;
    size_t newMeshletCount = 0;
    size_t newMeshletVertexCount = 0;
    size_t newMeshletTriangleCount = 0;

    for (const auto& mesh : meshes) {
      newVertexCount += mesh.positions.size();
      newIndexCount += mesh.indices.size();
      newMeshletCount += mesh.meshlets.size();
      newMeshletVertexCount += mesh.meshletVertices.size();
      newMeshletTriangleCount += mesh.meshletTriangles.size();
    }

    size_t newPositionsSize = newVertexCount * sizeof(glm::vec3);
    size_t newVertexAttribsSize = newVertexCount * sizeof(VertexAttribs);
    size_t newIndicesSize = newIndexCount * sizeof(uint32_t);
    size_t newMeshletsSize = newMeshletCount * sizeof(Meshlet);
    size_t newMeshletVerticesSize = newMeshletVertexCount * sizeof(uint32_t);
    size_t newMeshletTrianglesSize = newMeshletTriangleCount * sizeof(uint32_t);

    size_t totalPositionsSize = positionBuffer.occupied() + newPositionsSize;
    size_t totalVertexAttribsSize = attribBuffer.occupied() + newVertexAttribsSize;
    size_t totalIndicesSize = indexBuffer.occupied() + newIndicesSize;
    size_t totalMeshletsSize = meshletBuffer.occupied() + newMeshletsSize;
    size_t totalMeshletVerticesSize = meshletVertexBuffer.occupied() + newMeshletVerticesSize;
    size_t totalMeshletTrianglesSize = meshletTriangleBuffer.occupied() + newMeshletTrianglesSize;

    VK_DEBUG("Ensuring buffers are large enough for mesh upload:");
    VK_DEBUG("  Vertex Position Buffer: {} bytes -> {} bytes (+{})", positionBuffer.buffer.size(), totalPositionsSize,
             totalPositionsSize - positionBuffer.buffer.size());
    VK_DEBUG("  Vertex Attrib Buffer: {} bytes -> {} bytes (+{})", attribBuffer.buffer.size(), totalVertexAttribsSize,
             totalVertexAttribsSize - attribBuffer.buffer.size());
    VK_DEBUG("  Index Buffer: {} bytes -> {} bytes (+{})", indexBuffer.buffer.size(), totalIndicesSize,
             totalIndicesSize - indexBuffer.buffer.size());
    VK_DEBUG("  Meshlet Buffer: {} bytes -> {} bytes (+{})", meshletBuffer.buffer.size(), totalMeshletsSize,
             totalMeshletsSize - meshletBuffer.buffer.size());
    VK_DEBUG("  Meshlet Vertex Buffer: {} bytes -> {} bytes (+{})", meshletVertexBuffer.buffer.size(), totalMeshletVerticesSize,
             totalMeshletVerticesSize - meshletVertexBuffer.buffer.size());
    VK_DEBUG("  Meshlet Triangle Buffer: {} bytes -> {} bytes (+{})", meshletTriangleBuffer.buffer.size(), totalMeshletTrianglesSize,
             totalMeshletTrianglesSize - meshletTriangleBuffer.buffer.size());

    std::vector<Buffer> reallocatedBuffers;
    VKH_MAKE(
        oldPosBuf,
        realloc(positionBuffer, device, allocator, totalPositionsSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, "Mesh vertex position buffer"),
        "Failed to reallocate vertex position buffer");
    if (oldPosBuf) {
      reallocatedBuffers.push_back(std::move(*oldPosBuf));
    }

    VKH_MAKE(
        oldAttribBuf,
        realloc(attribBuffer, device, allocator, totalVertexAttribsSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, "Mesh vertex attrib buffer"),
        "Failed to reallocate vertex attrib buffer");
    if (oldAttribBuf) {
      reallocatedBuffers.push_back(std::move(*oldAttribBuf));
    }

    VKH_MAKE(oldIndexBuf, realloc(indexBuffer, device, allocator, totalIndicesSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, "Mesh index buffer"),
             "Failed to reallocate index buffer");
    if (oldIndexBuf) {
      reallocatedBuffers.push_back(std::move(*oldIndexBuf));
    }

    VKH_MAKE(oldMeshletBuf,
             realloc(meshletBuffer, device, allocator, totalMeshletsSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "Mesh meshlet buffer"),
             "Failed to reallocate meshlet buffer");
    if (oldMeshletBuf) {
      reallocatedBuffers.push_back(std::move(*oldMeshletBuf));
    }

    VKH_MAKE(oldMeshletVertexBuf,
             realloc(meshletVertexBuffer, device, allocator, totalMeshletVerticesSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     "Mesh meshlet vertex buffer"),
             "Failed to reallocate meshlet vertex buffer");
    if (oldMeshletVertexBuf) {
      reallocatedBuffers.push_back(std::move(*oldMeshletVertexBuf));
    }

    VKH_MAKE(oldMeshletTriangleBuf,
             realloc(meshletTriangleBuffer, device, allocator, totalMeshletTrianglesSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     "Mesh meshlet triangle buffer"),
             "Failed to reallocate meshlet triangle buffer");
    if (oldMeshletTriangleBuf) {
      reallocatedBuffers.push_back(std::move(*oldMeshletTriangleBuf));
    }

    return std::move(reallocatedBuffers);
  }
} // namespace kt::vkh::loading