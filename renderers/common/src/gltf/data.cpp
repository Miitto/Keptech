#include "keptech/rendering/gltf/data.hpp"

#include "keptech/core/fastgltf_formatting.hpp"
#include "keptech/core/kt-logger.hpp"
#include "keptech/rendering/constants.hpp"
#include <execution>
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/util.hpp>
#include <keptech/core/fastgltf_formatting.hpp>
#include <keptech/core/profile.hpp>
#include <meshoptimizer.h>
#include <ranges>

namespace kt::gltf {
  namespace {
    void loadMeshData(fastgltf::Asset& asset, Data& gltf) {
      KT_PROFILE_FUNCTION
      size_t startMeshCount = gltf.meshes.size();
      gltf.meshes.resize(startMeshCount + asset.meshes.size());

      auto enumView = std::views::enumerate(asset.meshes);

      struct Vertex {
        glm::vec3 position;
        glm::vec2 uv;
        glm::vec3 normal;
        glm::vec4 tangent;
      };

      std::for_each(std::execution::par, enumView.begin(), enumView.end(), [&](const std::tuple<size_t, fastgltf::Mesh&>& meshTuple) {
        auto& [meshIndex, mesh] = meshTuple;
        std::vector<Vertex> meshVertices;
        std::vector<Submesh> submeshes;
        std::vector<Meshlet> meshMeshlets;
        std::vector<uint32_t> meshMeshletVertices;
        std::vector<uint32_t> meshMeshletTriangles;

        submeshes.reserve(mesh.primitives.size());

        for (auto& primitive : mesh.primitives) {
          Submesh submesh{
              .vertexOffset = static_cast<uint32_t>(meshVertices.size()),
              .materialIndex = static_cast<uint32_t>(primitive.materialIndex.value_or(0)),
              .meshletOffset = static_cast<uint32_t>(meshMeshlets.size()),
              .meshletVertexOffset = static_cast<uint32_t>(meshMeshletVertices.size()),
              .meshletTriangleOffset = static_cast<uint32_t>(meshMeshletTriangles.size()),
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

          std::vector<Vertex> vertices;
          std::vector<uint32_t> indices;

          // Indices
          {
            auto& indicesAccessor = asset.accessors[primitive.indicesAccessor.value()];
            indices.reserve(indicesAccessor.count);

            fastgltf::iterateAccessor<uint32_t>(asset, indicesAccessor, [&](uint32_t index) { indices.push_back(index); });
          }

          // Positions
          {
            auto& posAccessor = asset.accessors[primitive.findAttribute("POSITION")->accessorIndex];
            vertices.resize(posAccessor.count);

            submeshes.back().vertexCount = static_cast<uint32_t>(vertices.size());

            glm::vec3 minPos{std::numeric_limits<float>::max()};
            glm::vec3 maxPos{std::numeric_limits<float>::lowest()};
            fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, posAccessor, [&](glm::vec3 position, size_t index) {
              position.x = -position.x;
              vertices[index].position = position;

              minPos.x = std::min(minPos.x, position.x);
              minPos.y = std::min(minPos.y, position.y);
              minPos.z = std::min(minPos.z, position.z);
              maxPos.x = std::max(maxPos.x, position.x);
              maxPos.y = std::max(maxPos.y, position.y);
              maxPos.z = std::max(maxPos.z, position.z);
            });

            submeshes.back().boundingSphere.center = (minPos + maxPos) * 0.5f;
            glm::vec3 maxExtent = abs(minPos);
            maxExtent.x = std::max(maxExtent.x, abs(maxPos.x));
            maxExtent.y = std::max(maxExtent.y, abs(maxPos.y));
            maxExtent.z = std::max(maxExtent.z, abs(maxPos.z));

            glm::vec3 radiusVec = maxExtent - submeshes.back().boundingSphere.center;
            float radius = glm::length(radiusVec);

            submeshes.back().boundingSphere.radius = radius;
          }

          // Normals
          {
            auto normals = primitive.findAttribute("NORMAL");
            if (normals != primitive.attributes.end()) {
              auto& normalAccessor = asset.accessors[normals->accessorIndex];

              fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, normalAccessor, [&](glm::vec3 normal, size_t index) {
                normal.x = -normal.x;
                vertices[index].normal = normal;
              });
            }
          }

          // UVs
          {
            auto uvs = primitive.findAttribute(fmt::format("TEXCOORD_{}", baseColorTexIndex));
            if (uvs != primitive.attributes.end()) {
              auto& uvAccessor = asset.accessors[uvs->accessorIndex];

              fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, uvAccessor,
                                                            [&](glm::vec2 uv, size_t index) { vertices[index].uv = uv; });
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
                vertices[index].tangent = tangent;
              });
            }
          }

          std::vector<uint32_t> remap(vertices.size());
          size_t vertexCount =
              meshopt_generateVertexRemap(remap.data(), indices.data(), indices.size(), vertices.data(), vertices.size(), sizeof(Vertex));

          std::vector<uint32_t> remappedIndices(indices.size());
          std::vector<Vertex> remappedVertices(vertexCount);

          meshopt_remapIndexBuffer(remappedIndices.data(), indices.data(), indices.size(), remap.data());
          meshopt_remapVertexBuffer(remappedVertices.data(), vertices.data(), vertices.size(), sizeof(Vertex), remap.data());

          meshopt_optimizeVertexCache(remappedIndices.data(), remappedIndices.data(), remappedIndices.size(), vertexCount);
          meshopt_optimizeOverdraw(remappedIndices.data(), remappedIndices.data(), remappedIndices.size(), &remappedVertices[0].position.x,
                                   vertexCount, sizeof(Vertex), 1.05f);
          meshopt_optimizeVertexFetch(remappedVertices.data(), remappedIndices.data(), remappedIndices.size(), remappedVertices.data(),
                                      remappedVertices.size(), sizeof(Vertex));

          size_t maxMeshlets =
              meshopt_buildMeshletsBound(remappedIndices.size(), constants::VERTICES_PER_MESHLET, constants::PRIMITIVES_PER_MESHLET);
          std::vector<meshopt_Meshlet> meshlets(maxMeshlets);
          std::vector<uint32_t> meshletVertices(remappedIndices.size());
          std::vector<uint8_t> meshletPrimitives8(remappedIndices.size());
          std::vector<uint32_t> meshletPrimitives(remappedIndices.size());

          size_t meshletCount = meshopt_buildMeshletsScan(meshlets.data(), meshletVertices.data(), meshletPrimitives8.data(),
                                                          remappedIndices.data(), remappedIndices.size(), vertexCount,
                                                          constants::VERTICES_PER_MESHLET, constants::PRIMITIVES_PER_MESHLET);

          const auto& last = meshlets[meshletCount - 1];

          meshletVertices.resize(last.vertex_offset + last.vertex_count);
          meshletPrimitives.resize(last.triangle_offset + last.triangle_count * 3);
          meshlets.resize(meshletCount);

          submeshes.back().meshletCount = static_cast<uint32_t>(meshletCount);
          submeshes.back().meshletVertexCount = static_cast<uint32_t>(meshletVertices.size());
          submeshes.back().meshletTriangleCount = static_cast<uint32_t>(meshletPrimitives.size());

          meshVertices.insert(meshVertices.end(), remappedVertices.begin(), remappedVertices.end());

          meshMeshlets.reserve(meshMeshlets.size() + meshletCount);
          for (const auto& meshlet : meshlets) {
            meshopt_optimizeMeshlet(&meshletVertices[meshlet.vertex_offset], &meshletPrimitives8[meshlet.triangle_offset],
                                    meshlet.triangle_count, meshlet.vertex_count);
          }

          for (size_t i = 0; i < meshletPrimitives8.size(); ++i) {
            meshletPrimitives[i] = meshletPrimitives8[i];
          }

          for (const auto& meshlet : meshlets) {
            auto bounds =
                meshopt_computeMeshletBounds(&meshletVertices[meshlet.vertex_offset], &meshletPrimitives8[meshlet.triangle_offset],
                                             meshlet.triangle_count, &remappedVertices[0].position.x, vertexCount, sizeof(Vertex));

            Meshlet m{
                .vertexOffset = static_cast<uint32_t>(meshlet.vertex_offset),
                .vertexCount = meshlet.vertex_count,
                .triangleOffset = static_cast<uint32_t>(meshlet.triangle_offset),
                .triangleCount = meshlet.triangle_count,
                .boundingSphere = {.center = {bounds.center[0], bounds.center[1], bounds.center[2]}, .radius = bounds.radius},
            };

            meshMeshlets.push_back(m);
          }

          meshMeshletVertices.insert(meshMeshletVertices.end(), meshletVertices.begin(), meshletVertices.end());
          meshMeshletTriangles.insert(meshMeshletTriangles.end(), meshletPrimitives.begin(), meshletPrimitives.end());
        }

        std::vector<glm::vec3> positions(meshVertices.size());
        std::vector<VertexAttribs> vertexAttribs(meshVertices.size());

        for (size_t i = 0; i < meshVertices.size(); ++i) {
          positions[i] = meshVertices[i].position;
          vertexAttribs[i] = VertexAttribs{
              .tangent = meshVertices[i].tangent,
              .normal = meshVertices[i].normal,
              .encodedUv = VertexAttribs::encodeUv(meshVertices[i].uv),
          };
        }

        MeshData meshData{
            .name = std::string(mesh.name),
            .positions = std::move(positions),
            .vertexAttribs = std::move(vertexAttribs),
            .submeshes = std::move(submeshes),
            .meshlets = std::move(meshMeshlets),
            .meshletVertices = std::move(meshMeshletVertices),
            .meshletTriangles = std::move(meshMeshletTriangles),
        };

        gltf.meshes[startMeshCount + meshIndex] = std::move(meshData);
      });

      KT_DEBUG("Loaded {} meshes", asset.meshes.size());
    }
  } // namespace

  std::expected<Data, std::string> Data::fromFile(std::string_view spath) {
    KT_PROFILE_FUNCTION

    fastgltf::Asset asset;
    Data loadedGltf{};
    {
      KT_PROFILE_SCOPE("GLTF File Read");
      std::filesystem::path path{spath};

      constexpr auto extensions = fastgltf::Extensions::KHR_texture_basisu | fastgltf::Extensions::KHR_materials_specular;
      constexpr auto options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
                               fastgltf::Options::LoadExternalBuffers | fastgltf::Options::GenerateMeshIndices |
                               fastgltf::Options::DecomposeNodeMatrices;
      fastgltf::Parser parser(extensions);

      KT_DEBUG("Loading glTF file from path: {}", path.string());
      auto gltfFile = fastgltf::MappedGltfFile::FromPath(path);
      if (!bool(gltfFile)) {
        return std::unexpected(fmt::format("Failed to load glTF file: {}", gltfFile.error()));
      }

      std::filesystem::path basePath = path.parent_path();

      KT_DEBUG("GLTF Base Path: {}", basePath.string());

      auto asset_res = parser.loadGltf(gltfFile.get(), basePath, options);
      if (!bool(asset_res)) {
        return std::unexpected(fmt::format("Failed to parse glTF file: {}", asset_res.error()));
      }

      asset = std::move(asset_res.get());

      loadedGltf.basePath = std::move(basePath);
    }

    loadMeshData(asset, loadedGltf);

    loadedGltf.materials = std::move(asset.materials);
    loadedGltf.textures = std::move(asset.textures);
    loadedGltf.images = std::move(asset.images);
    loadedGltf.samplers = std::move(asset.samplers);
    loadedGltf.bufferViews = std::move(asset.bufferViews);
    loadedGltf.buffers = std::move(asset.buffers);

    std::vector<Node> nodes;
    nodes.reserve(asset.nodes.size());

    {
      KT_PROFILE_SCOPE("Process Nodes");
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
    }

    KT_DEBUG("Loaded {} nodes", asset.nodes.size());

    return loadedGltf;
  }
} // namespace kt::gltf
