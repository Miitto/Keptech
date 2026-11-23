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
#include "keptech/rhi/sync.hpp"
#include "keptech/std/vec.hpp"

namespace kt::rhi {
  class ImageRef;
}
namespace kt::gltf {

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
    Vec<uint32_t> indices;
    Vec<glm::vec3> positions;
    Vec<kt::VertexAttribs> vertexAttribs;
    Vec<Submesh> submeshes;
    Vec<Meshlet> meshlets;
    Vec<uint32_t> meshletVertices;
    Vec<uint32_t> meshletTriangles;

    MeshSize getSize() const;
  };

  struct Data {
    struct Node {
      fastgltf::Node node;
      maths::Transform transform;
      uint32_t meshIndex = ~0u;
      Vec<Node> children{};
    };

    std::filesystem::path basePath;

    Vec<MeshData> meshes;
    Vec<fastgltf::Material> materials;
    Vec<fastgltf::Texture> textures;
    Vec<fastgltf::Image> images;
    Vec<fastgltf::Sampler> samplers;

    Vec<fastgltf::BufferView> bufferViews;
    Vec<fastgltf::Buffer> buffers;

    Vec<Node> roots;

    struct UploadResult {
      Scene scene;
      rhi::CopyWaitValue copyFenceValue = 0;
    };

    std::expected<UploadResult, std::string> upload() const;

    static std::expected<Data, std::string> fromFile(std::string_view path);

  private:
    template <typename T> struct PartialUploadResult {
      rhi::CopyWaitValue copyFenceValue = 0;
      T result;
    };

    std::expected<PartialUploadResult<Vec<rhi::ImageRef>>, std::string> uploadTextureData() const;
    std::expected<PartialUploadResult<Vec<Material>>, std::string> uploadMaterialData(const Vec<rhi::ImageRef>& textures) const;
    std::expected<PartialUploadResult<Vec<Mesh>>, std::string> uploadMeshData(const Vec<Material>& materials) const;
  };
} // namespace kt::gltf
