#pragma once

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#include <expected>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <string>

#include "keptech/gltf/scene.hpp"
#include "keptech/maths/sphere.hpp"
#include "keptech/maths/transform.hpp"
#include "keptech/mesh.hpp"
#include "keptech/rhi/wrappers/buffer.hpp"

namespace kt::gltf {
  class Scene;

  struct Submesh {
    struct BaseInfo {
      uint32_t count;
      /// Offset into the mesh's data
      uint32_t offset;
    };

    BaseInfo vertex;
    BaseInfo index;

    struct MeshletInfo {
      /// Number of meshlets in this submesh
      uint32_t count;
      /// Offset into the mesh's meshlet data
      uint32_t offset;
      /// Offset into the mesh's meshlet vertex data
      uint32_t vertexOffset;
      /// Offset into the mesh's meshlet triangle data
      uint32_t triangleOffset;
      /// Number of vertex indices in this submesh's meshlets
      uint32_t vertexCount;
      /// Number of triangle indices in this submesh's meshlets
      uint32_t triangleCount;
    } meshlet;

    uint32_t materialIndex;

    kt::maths::Sphere boundingSphere;
  };

  struct MeshSize {
    size_t positions;
    size_t vertexAttribs;
    size_t indices;
    size_t submeshes;
    size_t meshlets;
    size_t meshletVertices;
    size_t meshletTriangles;

    MeshSize& operator+=(const MeshSize& other) {
      positions += other.positions;
      vertexAttribs += other.vertexAttribs;
      indices += other.indices;
      submeshes += other.submeshes;
      meshlets += other.meshlets;
      meshletVertices += other.meshletVertices;
      meshletTriangles += other.meshletTriangles;

      return *this;
    }
  };

  struct MeshData {
    std::string name;
    std::vector<uint32_t> indices;
    std::vector<glm::vec3> positions;
    std::vector<kt::VertexAttribs> vertexAttribs;
    std::vector<Submesh> submeshes;
    std::vector<Meshlet> meshlets;
    std::vector<uint32_t> meshletVertices;
    std::vector<uint32_t> meshletTriangles;

    MeshSize getSize() const;
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

    struct UploadResult {
      Scene scene;
      uint64_t copyFenceValue;
      rhi::Buffer stagingBuffer;
    };

    std::expected<UploadResult, std::string> upload() const;

    static std::expected<Data, std::string> fromFile(std::string_view path);
  };
} // namespace kt::gltf
