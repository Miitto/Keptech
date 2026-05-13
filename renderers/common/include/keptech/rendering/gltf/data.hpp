#pragma once

#include "keptech/rendering/mesh.hpp"
#include <expected>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <keptech/maths/transform.hpp>
#include <string>

namespace kt::gltf {

  struct Submesh {
    /// Offset into the vertex buffer relative to the Mesh start
    uint32_t vertexOffset;
    uint32_t materialIndex;
    /// Number of meshlets in the submesh
    uint32_t meshletCount;
    /// Offset into the meshlet buffer relative to the Mesh start
    uint32_t meshletOffset;
    /// Offset of the first meshlet vertex relative to the Mesh start
    uint32_t meshletVertexOffset;
    /// Offset of the first meshlet triangle relative to the Mesh start
    uint32_t meshletTriangleOffset;

    uint32_t vertexCount;
    uint32_t meshletVertexCount;
    uint32_t meshletTriangleCount;

    kt::maths::Sphere boundingSphere;
  };

  struct MeshData {
    std::string name;
    std::vector<glm::vec3> positions;
    std::vector<kt::VertexAttribs> vertexAttribs;
    std::vector<Submesh> submeshes;
    std::vector<Meshlet> meshlets;
    std::vector<uint32_t> meshletVertices;
    std::vector<uint32_t> meshletTriangles;
  };

  struct Data {
    struct Node {
      fastgltf::Node node;
      maths::Transform transform;
      uint32_t meshIndex = ~0u;
      std::vector<Node> children{};
    };

    std::filesystem::path basePath;

    std::vector<MeshData> meshes;
    std::vector<fastgltf::Material> materials;
    std::vector<fastgltf::Texture> textures;
    std::vector<fastgltf::Image> images;
    std::vector<fastgltf::Sampler> samplers;

    std::vector<fastgltf::BufferView> bufferViews;
    std::vector<fastgltf::Buffer> buffers;

    std::vector<Node> roots;

    static std::expected<Data, std::string> fromFile(std::string_view path);
  };
} // namespace kt::gltf
