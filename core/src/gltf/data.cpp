#include "keptech/core/rendering/gltf/data.hpp"
#include "keptech/core/fastgltf_formatting.hpp"
#include "keptech/core/kt-logger.hpp"
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <keptech/core/fastgltf_formatting.hpp>

namespace keptech::gltf {
  namespace {
    void loadMeshData(fastgltf::Asset& asset, Data& gltf) {
      std::vector<Vertex> vertices;
      std::vector<uint32_t> indices;
      std::vector<Submesh> submeshes;
      for (auto& mesh : asset.meshes) {
        vertices.clear();
        indices.clear();
        submeshes.clear();

        for (auto& primitive : mesh.primitives) {
          Submesh submesh{
              .indexCount = static_cast<uint32_t>(
                  asset.accessors[primitive.indicesAccessor.value()].count),
              .indexOffset = static_cast<uint32_t>(indices.size()),
              .materialIndex =
                  static_cast<uint32_t>(primitive.materialIndex.value_or(0)),
          };
          submeshes.push_back(submesh);

          size_t startIndex = vertices.size();

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
            vertices.resize(vertices.size() +
                            static_cast<size_t>(posAccessor.count));

            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                asset, posAccessor, [&](glm::vec3 position, size_t index) {
                  Vertex vertex{};
                  vertex.position = position;
                  // Can flip Y here if needed in future
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
                    vertices[startIndex + index].color = glm::vec4(1.f);
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

        MeshData meshData{
            .name = std::string(mesh.name),
            .vertices = std::move(vertices),
            .indices = std::move(indices),
            .submeshes = std::move(submeshes),
        };

        gltf.meshes.emplace_back(std::move(meshData));
      }
    }
  } // namespace

  std::expected<Data, std::string> Data::fromFile(std::string_view spath) {

    std::filesystem::path path{spath};

    fastgltf::Parser parser{};

    constexpr auto options = fastgltf::Options::DontRequireValidAssetMember |
                             fastgltf::Options::AllowDouble |
                             fastgltf::Options::LoadExternalBuffers |
                             fastgltf::Options::LoadExternalImages |
                             fastgltf::Options::DecomposeNodeMatrices;

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

    Data loadedGltf{};

    loadMeshData(asset, loadedGltf);

    for (auto& material : asset.materials) {
      loadedGltf.materials.emplace_back(std::move(material));
    }

    for (auto& texture : asset.textures) {
      loadedGltf.textures.emplace_back(std::move(texture));
    }

    for (auto& image : asset.images) {
      loadedGltf.images.emplace_back(std::move(image));
    }

    for (auto& sampler : asset.samplers) {
      loadedGltf.samplers.emplace_back(std::move(sampler));
    }

    for (auto& bufferView : asset.bufferViews) {
      loadedGltf.bufferViews.emplace_back(std::move(bufferView));
    }

    for (auto& buffer : asset.buffers) {
      loadedGltf.buffers.emplace_back(std::move(buffer));
    }

    std::vector<Node> nodes;
    nodes.reserve(asset.nodes.size());

    for (auto& node : asset.nodes) {
      auto trs = std::get<fastgltf::TRS>(node.transform);
      glm::vec3 translation = glm::vec3(
          trs.translation.x(), trs.translation.y(), trs.translation.z());
      glm::quat rotation = glm::quat(trs.rotation.w(), trs.rotation.x(),
                                     trs.rotation.y(), trs.rotation.z());
      glm::vec3 scale = glm::vec3(trs.scale.x(), trs.scale.y(), trs.scale.z());

      maths::Transform transform(translation, rotation, scale);

      Node gltfNode{
          .node = node,
          .transform = transform,
          .meshIndex =
              static_cast<uint32_t>(node.meshIndex.value_or(UINT32_MAX)),
      };
      nodes.emplace_back(std::move(gltfNode));
    }

    std::vector<bool> isChildNode(nodes.size(), false);
    for (auto& node : nodes) {
      for (auto childIndex : node.node.children) {
        node.children.push_back(std::move(nodes[childIndex]));
        isChildNode[childIndex] = true;
      }
    }

    for (size_t i = 0; i < nodes.size(); ++i) {
      // If its been moved, means it's a child
      if (!isChildNode[i]) {
        loadedGltf.roots.emplace_back(std::move(nodes[i]));
      }
    }

    return loadedGltf;
  }
} // namespace keptech::gltf
