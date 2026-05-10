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
    uint32_t vertexOffset;
    uint32_t indexCount;
    uint32_t indexOffset;
    uint32_t materialIndex;
    uint32_t meshletCount;
    uint32_t meshletOffset;
    kt::maths::Sphere boundingSphere;
  };

  struct MeshData {
    std::string name;
    std::vector<glm::vec3> positions;
    std::vector<kt::VertexAttribs> vertexAttribs;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> shadowIndices;
    std::vector<Submesh> submeshes;
    std::vector<Meshlet> meshlets;
    std::vector<uint32_t> meshletVertices;
    std::vector<uint8_t> meshletTriangles;
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
