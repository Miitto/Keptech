#include "keptech/core/rendering/gltf/loaded.hpp"
#include "keptech/core/fastgltf_formatting.hpp"
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <keptech/core/fastgltf_formatting.hpp>

namespace keptech::core::gltf {
  namespace {
    void loadMeshData(fastgltf::Asset& asset, LoadedGltf& gltf) {

      for (auto& mesh : asset.meshes) {
        std::vector<rendering::Mesh::Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<rendering::Mesh::Submesh> submeshes;

        for (auto& primitive : mesh.primitives) {
          rendering::Mesh::Submesh submesh{
              .indexCount = static_cast<uint32_t>(
                  asset.accessors[primitive.indicesAccessor.value()].count),
              .indexOffset = static_cast<uint32_t>(indices.size()),
          };
          submeshes.push_back(submesh);

          size_t startIndex = indices.size();

          // Indices
          {
            auto& indicesAccessor =
                asset.accessors[primitive.indicesAccessor.value()];
            indices.reserve(indices.size() + indicesAccessor.count);

            fastgltf::iterateAccessor<uint32_t>(
                asset, indicesAccessor,
                [&](uint32_t index) { indices.push_back(index); });
          }

          // Positions
          {
            auto& posAccessor =
                asset.accessors[primitive.findAttribute("POSITION")
                                    ->accessorIndex];
            vertices.resize(std::max(vertices.size(),
                                     static_cast<size_t>(posAccessor.count)));

            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                asset, posAccessor, [&](glm::vec3 position, size_t index) {
                  rendering::Mesh::Vertex vertex{};
                  vertex.position = position;
                  vertex.position.y *= -1;
                  vertices[startIndex + index] = vertex;
                });
          }

          // Normals
          {
            auto normals = primitive.findAttribute("NORMAL");
            if (normals != primitive.attributes.end()) {
              auto& normalAccessor = asset.accessors[normals->accessorIndex];

              fastgltf::iterateAccessorWithIndex<glm::vec3>(
                  asset, normalAccessor, [&](glm::vec3 normal, size_t index) {
                    normal.y *= -1;
                    vertices[startIndex + index].normal = normal;
                  });
            }
          }

          // UVs
          {
            auto uvs = primitive.findAttribute("TEXCOORD_0");
            if (uvs != primitive.attributes.end()) {
              auto& uvAccessor = asset.accessors[uvs->accessorIndex];

              fastgltf::iterateAccessorWithIndex<glm::vec2>(
                  asset, uvAccessor, [&](glm::vec2 uv, size_t index) {
                    vertices[startIndex + index].uvX = uv.x;
                    vertices[startIndex + index].uvY = uv.y * -1;
                  });
            }
          }

          // Colors
          {
            auto colors = primitive.findAttribute("COLOR_0");
            if (colors != primitive.attributes.end()) {
              auto& colorAccessor = asset.accessors[colors->accessorIndex];

              fastgltf::iterateAccessorWithIndex<glm::vec4>(
                  asset, colorAccessor, [&](glm::vec4 color, size_t index) {
                    vertices[startIndex + index].color = color;
                  });
            }
          }

          // Tangents
          {
            auto tangents = primitive.findAttribute("TANGENT");
            if (tangents != primitive.attributes.end()) {
              auto& tangentAccessor = asset.accessors[tangents->accessorIndex];

              fastgltf::iterateAccessorWithIndex<glm::vec4>(
                  asset, tangentAccessor, [&](glm::vec4 tangent, size_t index) {
                    tangent.y *= -1;
                    vertices[startIndex + index].tangent = tangent;
                  });
            }
          }
        }

        rendering::MeshData meshData{
            .name = std::string(mesh.name),
            .vertices = std::move(vertices),
            .indices = std::move(indices),
            .submeshes = std::move(submeshes),
        };
        std::shared_ptr meshPtr =
            std::make_shared<rendering::MeshData>(std::move(meshData));

        gltf.meshses.emplace(meshPtr->name, meshPtr);
      }
    }
  } // namespace

  std::expected<LoadedGltf, std::string>
  LoadedGltf::fromFile(std::string_view spath) {

    std::filesystem::path path{spath};

    fastgltf::Parser parser{};

    constexpr auto options = fastgltf::Options::DontRequireValidAssetMember |
                             fastgltf::Options::AllowDouble |
                             fastgltf::Options::LoadExternalBuffers;

    auto gltfFile = fastgltf::MappedGltfFile::FromPath(path);
    if (!bool(gltfFile)) {
      return std::unexpected(
          fmt::format("Failed to load glTF file: {}", gltfFile.error()));
    }

    auto asset_res =
        parser.loadGltf(gltfFile.get(), path.parent_path(), options);
    if (!bool(asset_res)) {
      return std::unexpected(
          fmt::format("Failed to parse glTF file: {}", asset_res.error()));
    }

    auto& asset = asset_res.get();

    LoadedGltf loadedGltf{};

    loadMeshData(asset, loadedGltf);

    for (auto& material : asset.materials) {
      std::string materialName = std::string(material.name);
      loadedGltf.materials.emplace(
          materialName,
          std::make_shared<fastgltf::Material>(std::move(material)));
    }

    for (auto& texture : asset.textures) {
      std::string textureName = std::string(texture.name);
      loadedGltf.textures.emplace(
          textureName, std::make_shared<fastgltf::Texture>(std::move(texture)));
    }

    for (auto& image : asset.images) {
      std::string imageName = std::string(image.name);
      loadedGltf.images.emplace(
          imageName, std::make_shared<fastgltf::Image>(std::move(image)));
    }

    for (auto& sampler : asset.samplers) {
      std::string samplerName = std::string(sampler.name);
      loadedGltf.samplers.emplace(
          samplerName, std::make_shared<fastgltf::Sampler>(std::move(sampler)));
    }

    for (auto& node : asset.nodes) {
      std::string nodeName = std::string(node.name);
      loadedGltf.nodes.emplace(
          nodeName, std::make_shared<fastgltf::Node>(std::move(node)));
    }

    return loadedGltf;
  }
} // namespace keptech::core::gltf
