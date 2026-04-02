#include "keptech/rendering/gltf/data.hpp"
#include "keptech/core/fastgltf_formatting.hpp"
#include "keptech/core/kt-logger.hpp"
#include <execution>
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <keptech/core/fastgltf_formatting.hpp>
#include <ranges>

namespace kt::gltf {
  namespace {
    void loadMeshData(fastgltf::Asset& asset, Data& gltf) {

      size_t startMeshCount = gltf.meshes.size();
      gltf.meshes.resize(startMeshCount + asset.meshes.size());

      auto enumView = std::views::enumerate(asset.meshes);

      std::for_each(std::execution::par, enumView.begin(), enumView.end(), [&](const std::tuple<size_t, fastgltf::Mesh&>& meshTuple) {
        auto& [meshIndex, mesh] = meshTuple;
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<Submesh> submeshes;

        submeshes.reserve(mesh.primitives.size());

        for (auto& primitive : mesh.primitives) {
          Submesh submesh{
              .indexCount = static_cast<uint32_t>(asset.accessors[primitive.indicesAccessor.value()].count),
              .indexOffset = static_cast<uint32_t>(indices.size()),
              .materialIndex = static_cast<uint32_t>(primitive.materialIndex.value_or(0)),
          };
          submeshes.push_back(submesh);

          size_t baseColorTexIndex = 0;
          if (primitive.materialIndex.has_value()) {
            auto& material = asset.materials[primitive.materialIndex.value()];
            if (material.pbrData.baseColorTexture.has_value()) {
              auto& texInfo = material.pbrData.baseColorTexture.value();
              if (texInfo.transform && texInfo.transform->texCoordIndex.has_value()) {
                baseColorTexIndex = texInfo.transform->texCoordIndex.value();
              }
            }
          }

          size_t startIndex = vertices.size();

          // Indices
          {
            auto& indicesAccessor = asset.accessors[primitive.indicesAccessor.value()];
            indices.reserve(indices.size() + indicesAccessor.count);

            fastgltf::iterateAccessor<uint32_t>(asset, indicesAccessor,
                                                [&](uint32_t index) { indices.push_back(static_cast<uint32_t>(startIndex) + index); });
          }

          // Positions
          {
            auto& posAccessor = asset.accessors[primitive.findAttribute("POSITION")->accessorIndex];
            vertices.resize(vertices.size() + static_cast<size_t>(posAccessor.count));

            fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, posAccessor, [&](glm::vec3 position, size_t index) {
              position.x = -position.x;
              Vertex vertex{};
              vertex.position = position;
              vertices[startIndex + index] = vertex;
            });
          }

          // Normals
          {
            auto normals = primitive.findAttribute("NORMAL");
            if (normals != primitive.attributes.end()) {
              auto& normalAccessor = asset.accessors[normals->accessorIndex];

              fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, normalAccessor, [&](glm::vec3 normal, size_t index) {
                normal.x = -normal.x;
                vertices[startIndex + index].normal = normal;
              });
            }
          }

          // UVs
          {
            auto uvs = primitive.findAttribute(fmt::format("TEXCOORD_{}", baseColorTexIndex));
            if (uvs != primitive.attributes.end()) {
              auto& uvAccessor = asset.accessors[uvs->accessorIndex];

              fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, uvAccessor, [&](glm::vec2 uv, size_t index) {
                vertices[startIndex + index].uvX = uv.x;
                vertices[startIndex + index].uvY = uv.y;
              });
            }
          }

          // Colors
          {
            auto colors = primitive.findAttribute("COLOR_0");
            if (colors != primitive.attributes.end()) {
              auto& colorAccessor = asset.accessors[colors->accessorIndex];

              fastgltf::iterateAccessorWithIndex<glm::vec4>(
                  asset, colorAccessor, [&](glm::vec4 color, size_t index) { vertices[startIndex + index].color = color; });
            }
          }

          // Tangents
          {
            auto tangents = primitive.findAttribute("TANGENT");
            if (tangents != primitive.attributes.end()) {
              auto& tangentAccessor = asset.accessors[tangents->accessorIndex];

              fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, tangentAccessor, [&](glm::vec4 tangent, size_t index) {
                tangent.x = -tangent.x;
                tangent.w = -tangent.w; // Flip handedness
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

        gltf.meshes[startMeshCount + meshIndex] = std::move(meshData);
      });
    }
  } // namespace

  std::expected<Data, std::string> Data::fromFile(std::string_view spath) {

    std::filesystem::path path{spath};

    fastgltf::Parser parser{};

    constexpr auto options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
                             fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages |
                             fastgltf::Options::DecomposeNodeMatrices;

    auto gltfFile = fastgltf::MappedGltfFile::FromPath(path);
    if (!bool(gltfFile)) {
      return std::unexpected(fmt::format("Failed to load glTF file: {}", gltfFile.error()));
    }

    auto asset_res = parser.loadGltf(gltfFile.get(), path.parent_path(), options);
    if (!bool(asset_res)) {
      return std::unexpected(fmt::format("Failed to parse glTF file: {}", asset_res.error()));
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
      glm::vec3 translation = glm::vec3(-trs.translation.x(), trs.translation.y(), trs.translation.z());
      glm::quat backwardRotation = glm::quat(trs.rotation.w(), trs.rotation.x(), trs.rotation.y(), trs.rotation.z());
      glm::vec3 euler = glm::eulerAngles(backwardRotation);
      euler.z = -euler.z;
      glm::quat rotation = glm::quat(euler);
      glm::vec3 scale = glm::vec3(trs.scale.x(), trs.scale.y(), trs.scale.z());

      maths::Transform transform(translation, rotation, scale);

      Node gltfNode{
          .node = node,
          .transform = transform,
          .meshIndex = static_cast<uint32_t>(node.meshIndex.value_or(UINT32_MAX)),
      };

      if (node.lightIndex.has_value()) {
        gltfNode.lightIndex = static_cast<uint32_t>(node.lightIndex.value());
      }

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
} // namespace kt::gltf
