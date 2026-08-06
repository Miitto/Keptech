#include "dx-logger.hpp"
#include "renderer.hpp"

#include "keptech/render/gltf/data.hpp"
#include "keptech/render/gltf/scene.hpp"
#include "keptech/render/wrappers/subdivBuffer_impl.hpp"
#include "wrappers/bufferCreateInfo.hpp"
#include <expected>
#include <string_view>

namespace kt::rdr {
  namespace {}

  std::expected<gltf::Scene, std::string> Renderer::loadMesh(std::string_view path) {
    auto data_res = gltf::Data::fromFile(path);
    if (!data_res) {
      return std::unexpected(data_res.error());
    }
    auto& data = data_res.value();

    auto upload_res = uploadMeshes(data);
    if (!upload_res) {
      return std::unexpected(upload_res.error());
    }

    return gltf::Scene(data, upload_res.value());
  }

  std::expected<std::vector<Mesh>, std::string> Renderer::uploadMeshes(const gltf::Data& data) {
    std::vector<Mesh> meshes;
    meshes.reserve(data.meshes.size());

    auto resizeRes = ensureBuffersAreLargeEnough(data);
    if (!resizeRes) {
      return std::unexpected(resizeRes.error());
    }
    auto& sizes = resizeRes.value();

    size_t stagingSize = sizes.vertices * sizeof(glm::vec3) + sizes.vertices * sizeof(VertexAttribs) + sizes.indices * sizeof(uint32_t);
    auto stagingRes = Buffer::create(BufferCreateInfo(stagingSize, MappingMode::SeqWrite, MemoryUsage::PreferHost, "kt::staging"));
    if (!stagingRes) {
      return std::unexpected("Failed to create staging buffer");
    }
    auto& staging = stagingRes.value();
    DX_ASSERT(staging.isMapped(), "Staging buffer should be mapped for sequential write");

    size_t positionsSize = sizes.vertices * sizeof(glm::vec3);
    constexpr size_t positionsOffset = 0;
    size_t attribsSize = sizes.vertices * sizeof(VertexAttribs);
    size_t attribOffset = positionsSize;
    size_t indicesSize = sizes.indices * sizeof(uint32_t);
    size_t indexOffset = attribOffset + attribsSize;

    uint8_t* pos = static_cast<uint8_t*>(staging.mapping());
    uint8_t* attrib = pos + attribOffset;
    uint8_t* indices = pos + indexOffset;

    std::vector<Submesh> submeshes;

    for (const auto& mesh : data.meshes) {
      submeshes.clear();
      memcpy(pos, mesh.positions.data(), mesh.positions.size() * sizeof(glm::vec3));
      pos += mesh.positions.size() * sizeof(glm::vec3);
      memcpy(attrib, mesh.vertexAttribs.data(), mesh.vertexAttribs.size() * sizeof(VertexAttribs));
      attrib += mesh.vertexAttribs.size() * sizeof(VertexAttribs);
      memcpy(indices, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));
      indices += mesh.indices.size() * sizeof(uint32_t);

      for (const auto& [idx, primitive] : mesh.submeshes | std::views::enumerate) {
        Submesh submesh{
            .indexCount = primitive.index.count,
            .indexOffset = static_cast<uint32_t>(primitive.index.offset + m.buffers.indices.count()),
            .vertexOffset = static_cast<int32_t>(primitive.vertex.offset + m.buffers.positions.count()),
            .meshletOffset = static_cast<uint32_t>(primitive.meshlet.offset + 0), // TODO: meshlets
            .meshletCount = primitive.meshlet.count,
            .meshletVertexOffset = static_cast<uint32_t>(primitive.meshlet.vertexOffset + 0),
            .meshletTriangleOffset = static_cast<uint32_t>(primitive.meshlet.triangleOffset + 0),
            .vertexCount = primitive.vertex.count,
            .meshletVertexCount = primitive.meshlet.vertexCount,
            .meshletTriangleCount = primitive.meshlet.triangleCount,
            .boundingSphere = primitive.boundingSphere,
            .id = 0 + static_cast<uint32_t>(idx), // TODO: Mesh buffer
        };
        /* TODO: Materials
        if (primitive.materialIndex < materials.size()) {
          submesh.material = materials[primitive.materialIndex];
        }
        */
        submeshes.push_back(submesh);
      }
      meshes.emplace_back(static_cast<uint32_t>(mesh.positions.size()), submeshes, mesh.name);
    }

    resetCopyAllocator();

    auto& cmd = m.commandLists.copy;
    cmd->CopyBufferRegion(*m.buffers.positions, m.buffers.positions.occupied(), staging, positionsOffset, positionsSize);
    cmd->CopyBufferRegion(*m.buffers.vertexAttribs, m.buffers.vertexAttribs.occupied(), staging, attribOffset, attribsSize);
    cmd->CopyBufferRegion(*m.buffers.indices, m.buffers.indices.occupied(), staging, indexOffset, indicesSize);

    m.buffers.positions.registerWrite(sizes.vertices);
    m.buffers.vertexAttribs.registerWrite(sizes.vertices);
    m.buffers.indices.registerWrite(sizes.indices);

    cmd->Close();

    m.queues.copy->ExecuteCommandLists(1, cmd);

    m.copyFence.flush(m.queues.copy);

    return meshes;
  }

  std::expected<Renderer::NewSizes, std::string> Renderer::ensureBuffersAreLargeEnough(const gltf::Data& data) {
    size_t newVertices = 0;
    size_t newIndices = 0;

    for (const auto& mesh : data.meshes) {
      newVertices += mesh.positions.size();
      newIndices += mesh.indices.size();
    }

    size_t totalVertices = newVertices + m.buffers.positions.count();
    size_t totalIndices = newIndices + m.buffers.indices.count();

    resetCopyAllocator();

    auto& cmd = m.commandLists.copy;

    if (totalVertices > m.buffers.positions.capacity()) {
      auto newSize = std::max(totalVertices, m.buffers.positions.capacity() * 2);
      auto res =
          Buffer::create(BufferCreateInfo(newSize * sizeof(glm::vec3), MappingMode::None, MemoryUsage::PreferDevice, "kt::positions"));
      if (!res) {
        return std::unexpected("Failed to create new positions buffer");
      }
      auto& buf = res.value();
      cmd->CopyResource(buf, *m.buffers.positions);
      m.buffers.positions.getBuffer() = std::move(buf);
    }
    if (totalVertices > m.buffers.vertexAttribs.capacity()) {
      auto newSize = std::max(totalVertices, m.buffers.vertexAttribs.capacity() * 2);
      auto res = Buffer::create(
          BufferCreateInfo(newSize * sizeof(VertexAttribs), MappingMode::None, MemoryUsage::PreferDevice, "kt::vertexAttribs"));
      if (!res) {
        return std::unexpected("Failed to create new vertex attribs buffer");
      }
      auto& buf = res.value();
      cmd->CopyResource(buf, *m.buffers.vertexAttribs);
      m.buffers.vertexAttribs.getBuffer() = std::move(buf);
    }
    if (totalIndices > m.buffers.indices.capacity()) {
      auto newSize = std::max(totalIndices, m.buffers.indices.capacity() * 2);
      auto res = Buffer::create(BufferCreateInfo(newSize * sizeof(uint32_t), MappingMode::None, MemoryUsage::PreferDevice, "kt::indices"));
      if (!res) {
        return std::unexpected("Failed to create new indices buffer");
      }
      auto& buf = res.value();
      cmd->CopyResource(buf, *m.buffers.indices);
      m.buffers.indices.getBuffer() = std::move(buf);
    }

    cmd->Close();

    m.queues.copy->ExecuteCommandLists(1, cmd);

    m.copyFence.flush(m.queues.copy);

    return NewSizes{.vertices = newVertices, .indices = newIndices};
  }
} // namespace kt::rdr