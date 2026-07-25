#pragma once

#include "keptech/rendering/mesh.hpp"
#include "keptech/vulkan/wrappers/buffer.hpp"
#include <Volk/volk.h>
#include <expected>
#include <glm/fwd.hpp>
#include <vma/vk_mem_alloc.h>

namespace kt::gltf {
  struct MeshData;
};

namespace kt::vkh::loading {
  template <typename T> struct MaybeReallocResult {
    T result;
    std::vector<Buffer> reallocatedBuffers;
  };
  std::expected<MaybeReallocResult<uint32_t>, std::string> uploadVertices(const gltf::MeshData& data, const Device& device,
                                                                          SubdivBuffer<glm::vec3>& positionBuffer,
                                                                          SubdivBuffer<VertexAttribs>& attribBuffer);
  std::expected<MaybeReallocResult<uint32_t>, std::string> uploadIndices(const gltf::MeshData& data, const Device& device,
                                                                         SubdivBuffer<uint32_t>& indexBuffer);
  struct MeshletBufferOffsets {
    uint32_t meshlet;
    uint32_t vertex;
    uint32_t triangle;
  };
  std::expected<MaybeReallocResult<MeshletBufferOffsets>, std::string> uploadMeshlets(const gltf::MeshData& data, const Device& device,
                                                                                      SubdivBuffer<Meshlet>& meshletBuffer,
                                                                                      SubdivBuffer<uint32_t>& meshletVertexBuffer,
                                                                                      SubdivBuffer<uint32_t>& meshletTriangleBuffer);

  std::expected<std::vector<Buffer>, std::string>
  ensureBuffersAreLargeEnough(const Device& device, const std::vector<gltf::MeshData>& meshes, SubdivBuffer<glm::vec3>& positionBuffer,
                              SubdivBuffer<VertexAttribs>& attribBuffer, SubdivBuffer<uint32_t>& indexBuffer,
                              SubdivBuffer<Meshlet>& meshletBuffer, SubdivBuffer<uint32_t>& meshletVertexBuffer,
                              SubdivBuffer<uint32_t>& meshletTriangleBuffer);
} // namespace kt::vkh::loading