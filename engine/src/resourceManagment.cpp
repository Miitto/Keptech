#include "keptech/renderer.hpp"

#include <keptech/core/rendering/gltf/loaded.hpp>

namespace keptech {
  std::expected<Mesh, std::string> Renderer::loadMesh(const MeshData& data) {
    if (data.vertices.empty() || data.indices.empty()) {
      return std::unexpected("Mesh data contains no vertices or indices.");
    }

    auto cmdBufRes = backend->createCmdBuffer(CmdBufType::Compute);
    if (!cmdBufRes) {
      return std::unexpected(
          fmt::format("Failed to create command buffer for mesh loading: {}",
                      cmdBufRes.error()));
    }
    CmdBufPtr cmdBuf = std::move(cmdBufRes.value());

    size_t vertexSize = data.vertices.size() * sizeof(Vertex);
    size_t indexSize = data.indices.size() * sizeof(uint32_t);
    size_t totalSize = vertexSize + indexSize;

    BufferCreateInfo stagingBufInfo{
        .size = totalSize,
        .usage = BufferUsage::TransferSrc,
        .memoryType = BufferMemoryType::CpuToGpu,
    };

    auto stagingBufRes = backend->createBuffer(stagingBufInfo);
    if (!stagingBufRes) {
      return std::unexpected(
          fmt::format("Failed to create staging buffer for mesh loading: {}",
                      stagingBufRes.error()));
    }

    auto& stagingBuf = stagingBufRes.value();

    uint8_t* mapping = static_cast<uint8_t*>(stagingBuf->getMapping());

    memcpy(mapping, data.vertices.data(), vertexSize);
    mapping += vertexSize;
    memcpy(mapping, data.indices.data(), indexSize);

    cmdBuf->begin();
    cmdBuf->copyBufferToBuffer(*stagingBuf.get(), *buffers.vertex.get(),
                               vertexSize, 0, buffers.vertexEnd);
    cmdBuf->copyBufferToBuffer(*stagingBuf.get(), *buffers.index.get(),
                               indexSize, 0, buffers.indexEnd);
    cmdBuf->end();

    std::vector<CmdBufPtr> cmdBufs;
    cmdBufs.push_back(std::move(cmdBuf));
    backend->submitCommandBuffers(std::move(cmdBufs));

    Mesh mesh(buffers.indexEnd / sizeof(uint32_t), data.submeshes
#ifdef KT_ADD_RESOURCE_INFO
              ,
              data.name, data.vertices.size(), data.indices.size()
#endif
    );

    buffers.vertexEnd += vertexSize;
    buffers.indexEnd += indexSize;

    return mesh;
  }

  std::expected<std::vector<Mesh>, std::string>
  Renderer::loadMesh(std::string_view path) {
    auto res = gltf::LoadedGltf::fromFile(path);
    if (!res) {
      return std::unexpected(
          fmt::format("Failed to load glTF file '{}': {}", path, res.error()));
    }
    auto& gltf = res.value();

    std::vector<Mesh> meshes;

    for (auto& [name, meshData] : gltf.meshes) {
      auto meshRes = loadMesh(*meshData);
      if (!meshRes) {
        return std::unexpected(
            fmt::format("Failed to load mesh from glTF file '{}': {}", path,
                        meshRes.error()));
      }
      meshes.emplace_back(std::move(meshRes.value()));
    }

    return std::move(meshes);
  }
} // namespace keptech
