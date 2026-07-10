#include "keptech/rendering/gltf/data.hpp"

#include "keptech/core/kt-logger.hpp"
#include "keptech/rendering/constants.hpp"
#include <algorithm>
#include <execution>
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/util.hpp>
#include <keptech/core/fastgltf_formatting.hpp>
#include <keptech/core/profile.hpp>
#include <meshoptimizer.h>
#include <ranges>
#include <vector>

namespace kt::gltf {
  namespace {
    struct Vertex {
      glm::vec3 position;
      glm::vec2 uv;
      glm::vec3 normal;
      glm::vec4 tangent;
    };

    size_t findBaseColorTexIndex(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive) {
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
      return baseColorTexIndex;
    }

    void parseVertices(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, std::vector<uint32_t>& indices,
                       std::vector<Vertex>& vertices, Submesh& submesh, uint32_t baseColorTexIndex) {
      // Indices
      {
        auto& indicesAccessor = asset.accessors[primitive.indicesAccessor.value()];
        indices.reserve(indicesAccessor.count);
        submesh.index.count = static_cast<uint32_t>(indicesAccessor.count);

        fastgltf::iterateAccessor<uint32_t>(asset, indicesAccessor, [&](uint32_t index) { indices.push_back(index); });
      }

      // Positions
      {
        auto& posAccessor = asset.accessors[primitive.findAttribute("POSITION")->accessorIndex];
        vertices.resize(posAccessor.count);
        submesh.vertex.count = static_cast<uint32_t>(posAccessor.count);

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

        submesh.boundingSphere.center = (minPos + maxPos) * 0.5f;
        glm::vec3 maxExtent = abs(minPos);
        maxExtent.x = std::max(maxExtent.x, abs(maxPos.x));
        maxExtent.y = std::max(maxExtent.y, abs(maxPos.y));
        maxExtent.z = std::max(maxExtent.z, abs(maxPos.z));

        glm::vec3 radiusVec = maxExtent - submesh.boundingSphere.center;
        float radius = glm::length(radiusVec);

        submesh.boundingSphere.radius = radius;
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

          fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, uvAccessor, [&](glm::vec2 uv, size_t index) { vertices[index].uv = uv; });
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
    }

    struct OptimisedMeshData {
      std::vector<Vertex> vertices;
      std::vector<uint32_t> indices;
    };

    OptimisedMeshData optimiseMeshData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
      OptimisedMeshData result;

      std::vector<uint32_t> remap(vertices.size());
      size_t uniqueVertexCount =
          meshopt_generateVertexRemap(remap.data(), indices.data(), indices.size(), vertices.data(), vertices.size(), sizeof(Vertex));

      result.vertices.resize(uniqueVertexCount);
      result.indices.resize(indices.size());

      meshopt_remapIndexBuffer(result.indices.data(), indices.data(), indices.size(), remap.data());
      meshopt_remapVertexBuffer(result.vertices.data(), vertices.data(), vertices.size(), sizeof(Vertex), remap.data());

      meshopt_optimizeVertexCache(result.indices.data(), result.indices.data(), result.indices.size(), uniqueVertexCount);
      meshopt_optimizeOverdraw(result.indices.data(), result.indices.data(), result.indices.size(), &result.vertices[0].position.x,
                               uniqueVertexCount, sizeof(Vertex), 1.05f);
      meshopt_optimizeVertexFetch(result.vertices.data(), result.indices.data(), result.indices.size(), result.vertices.data(),
                                  result.vertices.size(), sizeof(Vertex));

      return result;
    }

    struct MeshletData {
      std::vector<Meshlet> meshlets;
      std::vector<uint32_t> vertices;
      std::vector<uint8_t> triangles;
    };

    MeshletData generateMeshlets(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
      std::vector<meshopt_Meshlet> meshlets;
      MeshletData result;

      size_t maxMeshlets = meshopt_buildMeshletsBound(indices.size(), constants::VERTICES_PER_MESHLET, constants::PRIMITIVES_PER_MESHLET);
      meshlets.resize(maxMeshlets);
      result.vertices.resize(indices.size());
      result.triangles.resize(indices.size());

      size_t meshletCount =
          meshopt_buildMeshletsScan(meshlets.data(), result.vertices.data(), result.triangles.data(), indices.data(), indices.size(),
                                    vertices.size(), constants::VERTICES_PER_MESHLET, constants::PRIMITIVES_PER_MESHLET);

      const auto& last = meshlets[meshletCount - 1];

      result.vertices.resize(last.vertex_offset + last.vertex_count);
      result.triangles.resize(last.triangle_offset + last.triangle_count * 3);
      meshlets.resize(meshletCount);

      for (const auto& meshlet : meshlets) {
        meshopt_optimizeMeshlet(&result.vertices[meshlet.vertex_offset], &result.triangles[meshlet.triangle_offset], meshlet.triangle_count,
                                meshlet.vertex_count);

        auto bounds = meshopt_computeMeshletBounds(&result.vertices[meshlet.vertex_offset], &result.triangles[meshlet.triangle_offset],
                                                   meshlet.triangle_count, &vertices[0].position.x, vertices.size(), sizeof(Vertex));

        Meshlet m{
            .vertexOffset = static_cast<uint32_t>(meshlet.vertex_offset),
            .vertexCount = meshlet.vertex_count,
            .triangleOffset = static_cast<uint32_t>(meshlet.triangle_offset),
            .triangleCount = meshlet.triangle_count,
            .boundingSphere = {.center = {bounds.center[0], bounds.center[1], bounds.center[2]}, .radius = bounds.radius},
        };

        result.meshlets.push_back(m);
      }

      return result;
    }

    struct PrimativeData {
      Submesh submesh{};
      OptimisedMeshData mesh{};
      MeshletData meshlet{};
    };

    PrimativeData processPrimitive(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, const MeshData& data) {
      std::vector<Vertex> vertices;
      std::vector<uint32_t> indices;

      /// Offset into the mesh's vertex buffer for this submesh
      uint32_t vertexOffset = static_cast<uint32_t>(data.positions.size());
      /// Offset into the mesh's index buffer for this submesh
      uint32_t indexOffset = static_cast<uint32_t>(data.vertexAttribs.size());
      /// Offset into the mesh's meshlet buffer for this submesh
      uint32_t meshletOffset = static_cast<uint32_t>(data.meshlets.size());
      /// Offset into the mesh's meshlet vertex buffer for this submesh
      uint32_t meshletVertexOffset = static_cast<uint32_t>(data.meshletVertices.size());
      /// Offset into the mesh's meshlet triangle buffer for this submesh
      uint32_t meshletTriangleOffset = static_cast<uint32_t>(data.meshletTriangles.size());

      Submesh submesh{
          .vertex =
              {
                  .offset = vertexOffset,
              },
          .index =
              {
                  .offset = indexOffset,
              },
          .meshlet =
              {
                  .offset = meshletOffset,
                  .vertexOffset = meshletVertexOffset,
                  .triangleOffset = meshletTriangleOffset,
              },
          .materialIndex = static_cast<uint32_t>(primitive.materialIndex.value_or(0)),
      };

      size_t baseColorTexIndex = findBaseColorTexIndex(asset, primitive);

      parseVertices(asset, primitive, indices, vertices, submesh, baseColorTexIndex);

      auto optimisedData = optimiseMeshData(vertices, indices);

      auto meshletData = generateMeshlets(optimisedData.vertices, optimisedData.indices);

      submesh.meshlet.count = static_cast<uint32_t>(meshletData.meshlets.size());
      submesh.meshlet.vertexCount = static_cast<uint32_t>(meshletData.vertices.size());
      submesh.meshlet.triangleCount = static_cast<uint32_t>(meshletData.triangles.size());

      return PrimativeData{
          .submesh = submesh,
          .mesh = std::move(optimisedData),
          .meshlet = std::move(meshletData),
      };
    }

    void loadMeshData(fastgltf::Asset& asset, Data& gltf) {
      KT_PROFILE_FUNCTION
      size_t startMeshCount = gltf.meshes.size();
      gltf.meshes.resize(startMeshCount + asset.meshes.size());

      auto enumView = std::views::enumerate(asset.meshes);

      std::for_each(std::execution::par, enumView.begin(), enumView.end(), [&](const std::tuple<size_t, fastgltf::Mesh&>& meshTuple) {
        auto& [meshIndex, mesh] = meshTuple;
        MeshData data{
            .name = std::string(mesh.name),
        };

        data.submeshes.reserve(mesh.primitives.size());

        for (auto& primitive : mesh.primitives) {
          auto prim = processPrimitive(asset, primitive, data);

          // Write out base mesh data
          data.indices.insert(data.indices.end(), prim.mesh.indices.begin(), prim.mesh.indices.end());
          data.positions.reserve(data.positions.size() + prim.mesh.vertices.size());
          data.vertexAttribs.reserve(data.vertexAttribs.size() + prim.mesh.vertices.size());
          for (const auto& vertex : prim.mesh.vertices) {
            data.positions.push_back(vertex.position);
            data.vertexAttribs.push_back(VertexAttribs{
                .tangent = vertex.tangent,
                .normal = vertex.normal,
                .encodedUv = VertexAttribs::encodeUv(vertex.uv),
            });
          }

          std::vector<uint32_t> meshletTriangles32(prim.meshlet.triangles.size());
          std::ranges::transform(prim.meshlet.triangles, meshletTriangles32.begin(),
                                 [](uint8_t triangle) { return static_cast<uint32_t>(triangle); });

          // Write out meshlet data
          data.meshlets.reserve(data.meshlets.size() + prim.meshlet.meshlets.size());
          data.meshletVertices.reserve(data.meshletVertices.size() + prim.meshlet.vertices.size());
          data.meshletTriangles.reserve(data.meshletTriangles.size() + meshletTriangles32.size());
          data.meshlets.insert(data.meshlets.end(), prim.meshlet.meshlets.begin(), prim.meshlet.meshlets.end());
          data.meshletVertices.insert(data.meshletVertices.end(), prim.meshlet.vertices.begin(), prim.meshlet.vertices.end());
          data.meshletTriangles.insert(data.meshletTriangles.end(), meshletTriangles32.begin(), meshletTriangles32.end());
        }

        gltf.meshes[startMeshCount + meshIndex] = std::move(data);
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
