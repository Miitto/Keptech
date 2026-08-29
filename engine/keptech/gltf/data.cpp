#include "data.hpp"

#include "keptech/rhi/bufferUsage.hpp"
#include "keptech/rhi/rhi.hpp"

#include "buffers.hpp"
#include "keptech/core/kt-logger.hpp"
#include "keptech/rhi/bufferCreateInfo.hpp"
#include "keptech/rhi/image.hpp"
#include "keptech/rhi/rhi_impl.hpp"
#include "mesh.hpp"
#include "scene.hpp"
#include <__msvc_ostream.hpp>
#include <algorithm>
#include <execution>
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/util.hpp>
#include <keptech/core/fastgltf_formatting.hpp>
#include <keptech/core/profile.hpp>
#include <ktx.h>
#include <meshoptimizer.h>
#include <optional>
#include <ranges>
#include <stb/image.h>
#include <vector>

namespace kt::gltf {
  namespace {
    struct Vertex {
      glm::vec3 position = {};
      glm::vec2 uv = {};
      glm::vec3 normal = glm::vec3(0.f, 0.f, 1.f);
      glm::vec4 tangent = glm::vec4(1.f, 0.f, 0.f, 1.f);
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
          .materialIndex = static_cast<uint32_t>(primitive.materialIndex.value_or(~0u)),
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
            data.vertexAttribs.emplace_back(vertex.normal, vertex.uv, vertex.tangent);
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

          data.submeshes.push_back(prim.submesh);
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

  std::expected<Data::UploadResult, std::string> Data::upload() const {
    auto imagesRes = uploadTextureData();
    if (!imagesRes) {
      return std::unexpected(imagesRes.error());
    }

    auto materialsRes = uploadMaterialData(imagesRes.value().result);
    if (!materialsRes) {
      return std::unexpected(materialsRes.error());
    }

    auto meshUploadRes = uploadMeshData(materialsRes.value().result);
    if (!meshUploadRes) {
      return std::unexpected(meshUploadRes.error());
    }

    auto& meshUpload = meshUploadRes.value();

    Scene scene(*this, meshUpload.result, meshUpload.copyFenceValue);

    return UploadResult{
        .scene = std::move(scene),
        .copyFenceValue = meshUpload.copyFenceValue,
    };
  }

  std::expected<Data::PartialUploadResult<std::vector<Mesh>>, std::string>
  Data::uploadMeshData(const std::vector<Material>& materials) const {
    auto& bufs = kt::Buffers::get();
    MeshSize totalSize{};
    MeshSize counts{};
    for (const auto& mesh : meshes) {
      totalSize += mesh.getSize();
      counts.positions += mesh.positions.size();
      counts.vertexAttribs += mesh.vertexAttribs.size();
      counts.indices += mesh.indices.size();
      counts.submeshes += mesh.submeshes.size();
      counts.meshlets += mesh.meshlets.size();
      counts.meshletVertices += mesh.meshletVertices.size();
      counts.meshletTriangles += mesh.meshletTriangles.size();
    }

    size_t stagingSize = totalSize.positions + totalSize.vertexAttribs + totalSize.indices + totalSize.submeshes + totalSize.meshlets +
                         totalSize.meshletVertices + totalSize.meshletTriangles;

    if (stagingSize == 0) {
      return Data::PartialUploadResult<std::vector<Mesh>>{
          .copyFenceValue = 0,
          .result = {},
      };
    }

    std::optional<rhi::Buffer> oldPosBuf;
    std::optional<rhi::Buffer> oldVertexAttribsBuf;
    std::optional<rhi::Buffer> oldIndicesBuf;
    std::optional<rhi::Buffer> oldSubmeshesBuf;
    std::optional<rhi::Buffer> oldMeshletsBuf;
    std::optional<rhi::Buffer> oldMeshletVerticesBuf;
    std::optional<rhi::Buffer> oldMeshletTrianglesBuf;

    if (!bufs.positions.hasSpaceFor(counts.positions)) {
      auto newPosBufRes =
          rhi::Buffer::create({bufs.positions.occupied() + totalSize.positions, rhi::BufferUsage::Vertex | rhi::BufferUsage::TransferDst,
                               rhi::BufferType::Default, "GLTF Positions Buffer"});
      KT_ASSERT(newPosBufRes.isOk(), "Failed to reallocate positions buffer: {}", newPosBufRes.error());
      if (bufs.positions->isValid())
        oldPosBuf = std::move(bufs.positions.getBuffer());
      bufs.positions.getBuffer() = std::move(newPosBufRes.value());
    }

    if (!bufs.vertexAttribs.hasSpaceFor(counts.vertexAttribs)) {
      auto newVertexAttribsBufRes = rhi::Buffer::create({bufs.vertexAttribs.occupied() + totalSize.vertexAttribs,
                                                         rhi::BufferUsage::Vertex | rhi::BufferUsage::TransferDst, rhi::BufferType::Default,
                                                         "GLTF Vertex Attributes Buffer"});
      KT_ASSERT(newVertexAttribsBufRes.isOk(), "Failed to reallocate vertex attributes buffer: {}", newVertexAttribsBufRes.error());
      if (bufs.vertexAttribs->isValid())
        oldVertexAttribsBuf = std::move(bufs.vertexAttribs.getBuffer());
      bufs.vertexAttribs.getBuffer() = std::move(newVertexAttribsBufRes.value());
    }

    if (!bufs.indices.hasSpaceFor(counts.indices)) {
      auto newIndicesBufRes =
          rhi::Buffer::create({bufs.indices.occupied() + totalSize.indices, rhi::BufferUsage::Index | rhi::BufferUsage::TransferDst,
                               rhi::BufferType::Default, "GLTF Indices Buffer"});
      KT_ASSERT(newIndicesBufRes.isOk(), "Failed to reallocate indices buffer: {}", newIndicesBufRes.error());
      if (bufs.indices->isValid())
        oldIndicesBuf = std::move(bufs.indices.getBuffer());
      bufs.indices.getBuffer() = std::move(newIndicesBufRes.value());
    }

    if (!bufs.submeshes.hasSpaceFor(counts.submeshes)) {
      auto newSubmeshesBufRes =
          rhi::Buffer::create({bufs.submeshes.occupied() + totalSize.submeshes, rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst,
                               rhi::BufferType::Default, "GLTF Submeshes Buffer"});
      KT_ASSERT(newSubmeshesBufRes.isOk(), "Failed to reallocate submeshes buffer: {}", newSubmeshesBufRes.error());
      if (bufs.submeshes->isValid())
        oldSubmeshesBuf = std::move(bufs.submeshes.getBuffer());
      bufs.submeshes.getBuffer() = std::move(newSubmeshesBufRes.value());
    }

    if (!bufs.meshlets.hasSpaceFor(counts.meshlets)) {
      auto newMeshletsBufRes =
          rhi::Buffer::create({bufs.meshlets.occupied() + totalSize.meshlets, rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst,
                               rhi::BufferType::Default, "GLTF Meshlets Buffer"});
      KT_ASSERT(newMeshletsBufRes.isOk(), "Failed to reallocate meshlets buffer: {}", newMeshletsBufRes.error());
      if (bufs.meshlets->isValid())
        oldMeshletsBuf = std::move(bufs.meshlets.getBuffer());
      bufs.meshlets.getBuffer() = std::move(newMeshletsBufRes.value());
    }

    if (!bufs.meshletVertices.hasSpaceFor(counts.meshletVertices)) {
      auto newMeshletVerticesBufRes = rhi::Buffer::create({bufs.meshletVertices.occupied() + totalSize.meshletVertices,
                                                           rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst,
                                                           rhi::BufferType::Default, "GLTF Meshlet Vertices Buffer"});
      KT_ASSERT(newMeshletVerticesBufRes.isOk(), "Failed to reallocate meshlet vertices buffer: {}", newMeshletVerticesBufRes.error());
      if (bufs.meshletVertices->isValid())
        oldMeshletVerticesBuf = std::move(bufs.meshletVertices.getBuffer());
      bufs.meshletVertices.getBuffer() = std::move(newMeshletVerticesBufRes.value());
    }

    if (!bufs.meshletTriangles.hasSpaceFor(counts.meshletTriangles)) {
      auto newMeshletTrianglesBufRes = rhi::Buffer::create({bufs.meshletTriangles.occupied() + totalSize.meshletTriangles,
                                                            rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst,
                                                            rhi::BufferType::Default, "GLTF Meshlet Triangles Buffer"});
      KT_ASSERT(newMeshletTrianglesBufRes.isOk(), "Failed to reallocate meshlet triangles buffer: {}", newMeshletTrianglesBufRes.error());
      if (bufs.meshletTriangles->isValid())
        oldMeshletTrianglesBuf = std::move(bufs.meshletTriangles.getBuffer());
      bufs.meshletTriangles.getBuffer() = std::move(newMeshletTrianglesBufRes.value());
    }

    constexpr size_t positionsOffset = 0;
    size_t vertexAttribsOffset = positionsOffset + totalSize.positions;
    size_t indicesOffset = vertexAttribsOffset + totalSize.vertexAttribs;
    size_t submeshesOffset = indicesOffset + totalSize.indices;
    size_t meshletsOffset = submeshesOffset + totalSize.submeshes;
    size_t meshletVerticesOffset = meshletsOffset + totalSize.meshlets;
    size_t meshletTrianglesOffset = meshletVerticesOffset + totalSize.meshletVertices;

    auto stagingRes = rhi::Buffer::create({stagingSize, rhi::BufferUsage::TransferSrc, rhi::BufferType::Staging, "GLTF Staging Buffer"});
    if (!stagingRes) {
      return std::unexpected(fmt::format("Failed to create staging buffer: {}", stagingRes.error()));
    }
    auto& staging = stagingRes.value();
    KT_ASSERT(staging.isMapped(), "Staging buffer should be mapped");

    glm::vec3* positionsPtr = staging.mapping<glm::vec3>(positionsOffset);
    VertexAttribs* vertexAttribsPtr = staging.mapping<VertexAttribs>(vertexAttribsOffset);
    uint32_t* indicesPtr = staging.mapping<uint32_t>(indicesOffset);
    GpuSubmesh* submeshesPtr = staging.mapping<GpuSubmesh>(submeshesOffset);
    Meshlet* meshletsPtr = staging.mapping<Meshlet>(meshletsOffset);
    uint32_t* meshletVerticesPtr = staging.mapping<uint32_t>(meshletVerticesOffset);
    uint32_t* meshletTrianglesPtr = staging.mapping<uint32_t>(meshletTrianglesOffset);

    std::vector<Mesh> resultMeshes;
    resultMeshes.reserve(meshes.size());

    for (const auto& mesh : meshes) {
      std::copy(mesh.positions.begin(), mesh.positions.end(), positionsPtr);
      positionsPtr += mesh.positions.size();

      std::copy(mesh.vertexAttribs.begin(), mesh.vertexAttribs.end(), vertexAttribsPtr);
      vertexAttribsPtr += mesh.vertexAttribs.size();

      std::copy(mesh.indices.begin(), mesh.indices.end(), indicesPtr);
      indicesPtr += mesh.indices.size();

      std::copy(mesh.meshlets.begin(), mesh.meshlets.end(), meshletsPtr);
      meshletsPtr += mesh.meshlets.size();

      std::copy(mesh.meshletVertices.begin(), mesh.meshletVertices.end(), meshletVerticesPtr);
      meshletVerticesPtr += mesh.meshletVertices.size();

      std::copy(mesh.meshletTriangles.begin(), mesh.meshletTriangles.end(), meshletTrianglesPtr);
      meshletTrianglesPtr += mesh.meshletTriangles.size();

      std::vector<GpuSubmesh> submeshes(mesh.submeshes.size());
      std::ranges::transform(mesh.submeshes, submeshes.begin(), [&](const Submesh& submesh) {
        GpuSubmesh gpuSubmesh{};
        gpuSubmesh.vertexOffset = submesh.vertex.offset;
        gpuSubmesh.vertexCount = submesh.vertex.count;
        gpuSubmesh.indexOffset = submesh.index.offset;
        gpuSubmesh.indexCount = submesh.index.count;
        gpuSubmesh.meshletOffset = submesh.meshlet.offset;
        gpuSubmesh.meshletCount = submesh.meshlet.count;
        gpuSubmesh.meshletVertexOffset = submesh.meshlet.vertexOffset;
        gpuSubmesh.meshletVertexCount = submesh.meshlet.vertexCount;
        gpuSubmesh.meshletTriangleOffset = submesh.meshlet.triangleOffset;
        gpuSubmesh.meshletTriangleCount = submesh.meshlet.triangleCount;
        gpuSubmesh.materialIndex = submesh.materialIndex;
        gpuSubmesh.boundingSphere = submesh.boundingSphere;
        gpuSubmesh.materialIndex = submesh.materialIndex == ~0u ? ~0u : materials[submesh.materialIndex].id;
        return gpuSubmesh;
      });
      std::copy(submeshes.begin(), submeshes.end(), submeshesPtr);
      submeshesPtr += submeshes.size();

      std::vector<kt::Submesh> meshSubmeshes(mesh.submeshes.size());
      uint32_t meshId = static_cast<uint32_t>(bufs.submeshes.count());
      std::ranges::transform(mesh.submeshes, meshSubmeshes.begin(), [&](const Submesh& in) {
        kt::Submesh submesh{
            .indexCount = in.index.count,
            .indexOffset = static_cast<uint32_t>(in.index.offset + bufs.vertexAttribs.count()),
            .vertexOffset = static_cast<int32_t>(in.vertex.offset + bufs.positions.count()),
            .meshletOffset = static_cast<uint32_t>(in.meshlet.offset + bufs.meshlets.count()),
            .meshletCount = in.meshlet.count,
            .meshletVertexOffset = static_cast<uint32_t>(in.meshlet.vertexOffset + bufs.meshletVertices.count()),
            .meshletTriangleOffset = static_cast<uint32_t>(in.meshlet.triangleOffset + bufs.meshletTriangles.count()),
            .vertexCount = in.vertex.count,
            .material = in.materialIndex == ~0u ? Material{} : materials[in.materialIndex],
            .boundingSphere = in.boundingSphere,
            .id = meshId++,
        };
        return submesh;
      });

      resultMeshes.emplace_back(mesh.name, std::move(meshSubmeshes));
    }

    auto copyValue = rhi::RHI::get().oneshotCopy([&](rhi::CommandBuffer& cmd) {
      cmd.copyBufferRegion(bufs.positions, staging, bufs.positions.occupied(), positionsOffset, totalSize.positions);
      bufs.positions.registerWrites(counts.positions);
      cmd.copyBufferRegion(bufs.vertexAttribs, staging, bufs.vertexAttribs.occupied(), vertexAttribsOffset, totalSize.vertexAttribs);
      bufs.vertexAttribs.registerWrites(counts.vertexAttribs);
      cmd.copyBufferRegion(bufs.indices, staging, bufs.indices.occupied(), indicesOffset, totalSize.indices);
      bufs.indices.registerWrites(counts.indices);
      cmd.copyBufferRegion(bufs.submeshes, staging, bufs.submeshes.occupied(), submeshesOffset, totalSize.submeshes);
      bufs.submeshes.registerWrites(counts.submeshes);
      cmd.copyBufferRegion(bufs.meshlets, staging, bufs.meshlets.occupied(), meshletsOffset, totalSize.meshlets);
      bufs.meshlets.registerWrites(counts.meshlets);
      cmd.copyBufferRegion(bufs.meshletVertices, staging, bufs.meshletVertices.occupied(), meshletVerticesOffset,
                           totalSize.meshletVertices);
      bufs.meshletVertices.registerWrites(counts.meshletVertices);
      cmd.copyBufferRegion(bufs.meshletTriangles, staging, bufs.meshletTriangles.occupied(), meshletTrianglesOffset,
                           totalSize.meshletTriangles);
      bufs.meshletTriangles.registerWrites(counts.meshletTriangles);

      std::vector<rhi::Buffer> buffersToDrop;
      buffersToDrop.emplace_back(std::move(staging));

      if (oldPosBuf.has_value()) {
        cmd.copyBufferRegion(bufs.positions, *oldPosBuf, 0, 0, oldPosBuf->size());
        buffersToDrop.emplace_back(std::move(*oldPosBuf));
      }
      if (oldVertexAttribsBuf.has_value()) {
        cmd.copyBufferRegion(bufs.vertexAttribs, *oldVertexAttribsBuf, 0, 0, oldVertexAttribsBuf->size());
        buffersToDrop.emplace_back(std::move(*oldVertexAttribsBuf));
      }
      if (oldIndicesBuf.has_value()) {
        cmd.copyBufferRegion(bufs.indices, *oldIndicesBuf, 0, 0, oldIndicesBuf->size());
        buffersToDrop.emplace_back(std::move(*oldIndicesBuf));
      }
      if (oldSubmeshesBuf.has_value()) {
        cmd.copyBufferRegion(bufs.submeshes, *oldSubmeshesBuf, 0, 0, oldSubmeshesBuf->size());
        buffersToDrop.emplace_back(std::move(*oldSubmeshesBuf));
      }
      if (oldMeshletsBuf.has_value()) {
        cmd.copyBufferRegion(bufs.meshlets, *oldMeshletsBuf, 0, 0, oldMeshletsBuf->size());
        buffersToDrop.emplace_back(std::move(*oldMeshletsBuf));
      }
      if (oldMeshletVerticesBuf.has_value()) {
        cmd.copyBufferRegion(bufs.meshletVertices, *oldMeshletVerticesBuf, 0, 0, oldMeshletVerticesBuf->size());
        buffersToDrop.emplace_back(std::move(*oldMeshletVerticesBuf));
      }
      if (oldMeshletTrianglesBuf.has_value()) {
        cmd.copyBufferRegion(bufs.meshletTriangles, *oldMeshletTrianglesBuf, 0, 0, oldMeshletTrianglesBuf->size());
        buffersToDrop.emplace_back(std::move(*oldMeshletTrianglesBuf));
      }

      return buffersToDrop;
    });

    KT_DEBUG("Uploaded {} meshes to GPU", meshes.size());

    return Data::PartialUploadResult<std::vector<Mesh>>{
        .copyFenceValue = copyValue,
        .result = std::move(resultMeshes),
    };
  }

  std::expected<Data::PartialUploadResult<std::vector<Material>>, std::string>
  Data::uploadMaterialData(const std::vector<rhi::ImageRef>& textures) const {
    auto& bufs = kt::Buffers::get();
    size_t totalSize = sizeof(GpuMaterial) * materials.size();

    if (totalSize == 0) {
      return Data::PartialUploadResult<std::vector<Material>>{
          .copyFenceValue = 0,
          .result = {},
      };
    }

    std::optional<rhi::Buffer> oldMaterialsBuf;
    if (!bufs.materials.hasSpaceFor(materials.size())) {
      auto newMaterialsBufRes =
          rhi::Buffer::create({bufs.materials.occupied() + totalSize, rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst,
                               rhi::BufferType::Default, "GLTF Materials Buffer"});
      KT_ASSERT(newMaterialsBufRes.isOk(), "Failed to reallocate materials buffer: {}", newMaterialsBufRes.error());
      if (bufs.materials->isValid())
        oldMaterialsBuf = std::move(bufs.materials.getBuffer());
      bufs.materials.getBuffer() = std::move(newMaterialsBufRes.value());
    }

    auto stagingRes =
        rhi::Buffer::create({totalSize, rhi::BufferUsage::TransferSrc, rhi::BufferType::Staging, "GLTF Materials Staging Buffer"});
    if (!stagingRes) {
      return std::unexpected(fmt::format("Failed to create staging buffer: {}", stagingRes.error()));
    }
    auto& staging = stagingRes.value();
    KT_ASSERT(staging.isMapped(), "Staging buffer should be mapped");

    auto getImage = [&](const std::optional<fastgltf::TextureInfo>& texInfo) -> rhi::ImageRef {
      if (!texInfo.has_value()) {
        return {};
      }
      return textures[this->textures[texInfo->textureIndex].imageIndex.value()];
    };

    auto getNormalImage = [&](const std::optional<fastgltf::NormalTextureInfo>& texInfo) -> rhi::ImageRef {
      if (!texInfo.has_value()) {
        return {};
      }
      return textures[this->textures[texInfo->textureIndex].imageIndex.value()];
    };
    auto getOcclusionImage = [&](const std::optional<fastgltf::OcclusionTextureInfo>& texInfo) -> rhi::ImageRef {
      if (!texInfo.has_value()) {
        return {};
      }
      return textures[this->textures[texInfo->textureIndex].imageIndex.value()];
    };

    GpuMaterial* materialsPtr = staging.mapping<GpuMaterial>(0);

    uint32_t materialId = static_cast<uint32_t>(bufs.materials.count());

    std::vector<Material> resultMaterials;
    resultMaterials.reserve(materials.size());
    for (const auto& material : materials) {
      Material resultMaterial{
          .albedo = getImage(material.pbrData.baseColorTexture),
          .bump = getNormalImage(material.normalTexture),
          .emissive = getImage(material.emissiveTexture),
          .metRough = getImage(material.pbrData.metallicRoughnessTexture),
          .ao = getOcclusionImage(material.occlusionTexture),
          .albedoFactor = glm::vec4(material.pbrData.baseColorFactor.x(), material.pbrData.baseColorFactor.y(),
                                    material.pbrData.baseColorFactor.z(), material.pbrData.baseColorFactor.w()),
          .emissiveFactor = glm::vec3(material.emissiveFactor.x(), material.emissiveFactor.y(), material.emissiveFactor.z()),
          .metFactor = material.pbrData.metallicFactor,
          .roughFactor = material.pbrData.roughnessFactor,
          .specFactor = material.specular ? material.specular->specularFactor : 0.5f,
          .alphaCutoff = material.alphaCutoff,
          .alphaMode = static_cast<AlphaMode>(material.alphaMode),
          .doubleSided = material.doubleSided,
          .id = materialId++,
      };
      GpuMaterial gpuMaterial{
          .albedo = resultMaterial.albedo.getTextureIndex(),
          .bump = resultMaterial.bump.getTextureIndex(),
          .emissive = resultMaterial.emissive.getTextureIndex(),
          .metRough = resultMaterial.metRough.getTextureIndex(),
          .ao = resultMaterial.ao.getTextureIndex(),
          .albedoFactor = glm::vec4(material.pbrData.baseColorFactor.x(), material.pbrData.baseColorFactor.y(),
                                    material.pbrData.baseColorFactor.z(), material.pbrData.baseColorFactor.w()),
          .emissiveFactor = glm::vec3(material.emissiveFactor.x(), material.emissiveFactor.y(), material.emissiveFactor.z()),
          .metFactor = material.pbrData.metallicFactor,
          .roughFactor = material.pbrData.roughnessFactor,
          .specFactor = material.specular ? material.specular->specularFactor : 0.5f,
          .alphaCutoff = material.alphaCutoff,
          .alphaMode = static_cast<uint32_t>(material.alphaMode),
      };

      *materialsPtr++ = gpuMaterial;
      resultMaterials.emplace_back(resultMaterial);
    }

    auto copyValue = rhi::RHI::get().oneshotCopy([&](rhi::CommandBuffer& cmd) {
      cmd.copyBufferRegion(bufs.materials, staging, bufs.materials.occupied(), 0, totalSize);
      bufs.materials.registerWrites(materials.size());

      std::vector<rhi::Buffer> buffersToDrop;
      buffersToDrop.emplace_back(std::move(staging));
      if (oldMaterialsBuf.has_value()) {
        cmd.copyBufferRegion(bufs.materials, *oldMaterialsBuf, 0, 0, oldMaterialsBuf->size());
        buffersToDrop.emplace_back(std::move(*oldMaterialsBuf));
      }

      return buffersToDrop;
    });

    return Data::PartialUploadResult<std::vector<Material>>{
        .copyFenceValue = copyValue,
        .result = std::move(resultMaterials),
    };
  }

  std::expected<Data::PartialUploadResult<std::vector<rhi::ImageRef>>, std::string> Data::uploadTextureData() const {
    // Alias map to map image indices to which image to load - for example we prefer to load the KTX2 image if it exists rather than the PNG
    // image.
    std::map<size_t, size_t> imageIndexMap;
    for (auto& material : materials) {
      if (material.pbrData.baseColorTexture.has_value()) {
        auto& tex = textures[material.pbrData.baseColorTexture->textureIndex];
        if (tex.basisuImageIndex.has_value()) {
          imageIndexMap[tex.imageIndex.value()] = tex.basisuImageIndex.value();
        } else if (tex.imageIndex.has_value()) {
          imageIndexMap[tex.imageIndex.value()] = tex.imageIndex.value();
        }
      }
      if (material.normalTexture.has_value()) {
        auto& tex = textures[material.normalTexture->textureIndex];
        if (tex.basisuImageIndex.has_value()) {
          imageIndexMap[tex.imageIndex.value()] = tex.basisuImageIndex.value();
        } else if (tex.imageIndex.has_value()) {
          imageIndexMap[tex.imageIndex.value()] = tex.imageIndex.value();
        }
      }
      if (material.emissiveTexture.has_value()) {
        auto& tex = textures[material.emissiveTexture->textureIndex];
        if (tex.basisuImageIndex.has_value()) {
          imageIndexMap[tex.imageIndex.value()] = tex.basisuImageIndex.value();
        } else if (tex.imageIndex.has_value()) {
          imageIndexMap[tex.imageIndex.value()] = tex.imageIndex.value();
        }
      }
      if (material.pbrData.metallicRoughnessTexture.has_value()) {
        auto& tex = textures[material.pbrData.metallicRoughnessTexture->textureIndex];
        if (tex.basisuImageIndex.has_value()) {
          imageIndexMap[tex.imageIndex.value()] = tex.basisuImageIndex.value();
        } else if (tex.imageIndex.has_value()) {
          imageIndexMap[tex.imageIndex.value()] = tex.imageIndex.value();
        }
      }
      if (material.occlusionTexture.has_value()) {
        auto& tex = textures[material.occlusionTexture->textureIndex];
        if (tex.basisuImageIndex.has_value()) {
          imageIndexMap[tex.imageIndex.value()] = tex.basisuImageIndex.value();
        } else if (tex.imageIndex.has_value()) {
          imageIndexMap[tex.imageIndex.value()] = tex.imageIndex.value();
        }
      }
    }

    if (imageIndexMap.empty()) {
      return Data::PartialUploadResult<std::vector<rhi::ImageRef>>{
          .copyFenceValue = 0,
          .result = {},
      };
    }

    struct Tex {
      const char* name;
      void* data;
      size_t size;
      rhi::ImageFormat format;
      glm::uvec2 dimensions;
      ktxTexture2* ktx2Texture;
    };

    std::vector<Tex> texturesToLoad(imageIndexMap.size());

    ktx_transcode_fmt_e transcodeFormat = KTX_TTF_BC7_RGBA;
    rhi::ImageFormat imageFormat = rhi::ImageFormat::BC7_UNORM;

    if (!rhi::RHI::get().canSampleFromFormat(imageFormat)) {
      if (rhi::RHI::get().canSampleFromFormat(rhi::ImageFormat::BC3_UNORM)) {
        transcodeFormat = KTX_TTF_BC3_RGBA;
        imageFormat = rhi::ImageFormat::BC3_UNORM;
      } else if (rhi::RHI::get().canSampleFromFormat(rhi::ImageFormat::R8G8B8A8_UNORM)) {
        transcodeFormat = KTX_TTF_RGBA32;
        imageFormat = rhi::ImageFormat::R8G8B8A8_UNORM;
      } else {
        KT_ABORT("No supported compressed texture format available for sampling");
      }
    }

    std::for_each(std::execution::par, imageIndexMap.begin(), imageIndexMap.end(), [&](const std::pair<size_t, size_t>& pair) {
      auto [index, imageIndex] = pair;
      auto& gltfImage = images[imageIndex];

      // PNG or something, use stb_image
      if (index == imageIndex) {
        auto fromMemory = [&](const std::span<const std::byte>& bytes) {
          stbi_set_flip_vertically_on_load(true);
          int width = 0, height = 0, channels = 0;
          auto* data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()), static_cast<int>(bytes.size()), &width,
                                             &height, &channels, 4);
          if (!data) {
            KT_ABORT("Failed to load image from memory: {}", stbi_failure_reason());
          }

          texturesToLoad[index] = Tex{
              .name = gltfImage.name.c_str(),
              .data = data,
              .size = static_cast<size_t>(width * height * 4),
              .format = rhi::ImageFormat::R8G8B8A8_UNORM,
              .dimensions = glm::uvec2(width, height),
              .ktx2Texture = nullptr,
          };
        };

        std::visit(fastgltf::visitor{
                       [](auto& arg) -> void { KT_ABORT("Unsupported image source type: {}", typeid(arg).name()); },
                       [&](const fastgltf::sources::Array& array) -> void { fromMemory(array.bytes); },
                       [&](const fastgltf::sources::URI& filePath) -> void {
                         std::filesystem::path path = basePath / filePath.uri.fspath();
                         stbi_set_flip_vertically_on_load(true);
                         int width = 0, height = 0, channels = 0;
                         auto* data = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
                         if (!data) {
                           KT_ABORT("Failed to load image from file: {}", stbi_failure_reason());
                         }
                         texturesToLoad[index] = Tex{
                             .name = gltfImage.name.c_str(),
                             .data = data,
                             .size = static_cast<size_t>(width * height * 4),
                             .format = rhi::ImageFormat::R8G8B8A8_UNORM,
                             .dimensions = glm::uvec2(width, height),
                             .ktx2Texture = nullptr,
                         };
                       },
                       [&](const fastgltf::sources::Vector& vector) -> void { fromMemory(vector.bytes); },
                       [&](const fastgltf::sources::BufferView view) -> void {
                         auto& bv = bufferViews[view.bufferViewIndex];
                         auto& buf = buffers[bv.bufferIndex];

                         std::visit(fastgltf::visitor{
                                        [](auto& arg) -> void { KT_ABORT("Unsupported buffer source type: {}", typeid(arg).name()); },
                                        [&](const fastgltf::sources::Array& array) -> void {
                                          fromMemory(std::span<const std::byte>(array.bytes.data() + bv.byteOffset, bv.byteLength));
                                        },
                                        [&](const fastgltf::sources::Vector& vector) -> void {
                                          fromMemory(std::span<const std::byte>(vector.bytes.data() + bv.byteOffset, bv.byteLength));
                                        },
                                    },
                                    buf.data);
                       },
                   },
                   gltfImage.data);
      }
      // KTX2 image
      else {
        auto fromMemory = [&](const std::span<const std::byte>& bytes) {
          ktxTexture2* ktx2Texture = nullptr;
          KTX_error_code result = ktxTexture2_CreateFromMemory(reinterpret_cast<const ktx_uint8_t*>(bytes.data()), bytes.size(),
                                                               KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx2Texture);
          if (result != KTX_SUCCESS) {
            KT_ABORT("Failed to load KTX2 image from memory: {}", ktxErrorString(result));
          }

          auto e = ktxTexture2_TranscodeBasis(ktx2Texture, transcodeFormat, 0);
          if (e != KTX_SUCCESS) {
            KT_ABORT("Failed to transcode KTX2 image: {}", ktxErrorString(e));
          }

          texturesToLoad[index] = Tex{
              .name = gltfImage.name.c_str(),
              .data = nullptr,
              .size = 0,
              .format = imageFormat,
              .dimensions = glm::uvec2(ktx2Texture->baseWidth, ktx2Texture->baseHeight),
              .ktx2Texture = ktx2Texture,
          };
        };

        std::visit(fastgltf::visitor{
                       [](auto& arg) -> void { KT_ABORT("Unsupported image source type: {}", typeid(arg).name()); },
                       [&](const fastgltf::sources::Array& array) -> void { fromMemory(array.bytes); },
                       [&](const fastgltf::sources::URI& filePath) -> void {
                         std::filesystem::path path = basePath / filePath.uri.fspath();

                         ktxTexture2* ktx2Texture = nullptr;
                         KTX_error_code result =
                             ktxTexture2_CreateFromNamedFile(path.string().c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx2Texture);
                         if (result != KTX_SUCCESS) {
                           KT_ABORT("Failed to load KTX2 image from file: {}", ktxErrorString(result));
                         }

                         auto e = ktxTexture2_TranscodeBasis(ktx2Texture, transcodeFormat, 0);
                         if (e != KTX_SUCCESS) {
                           KT_ABORT("Failed to transcode KTX2 image: {}", ktxErrorString(e));
                         }

                         texturesToLoad[index] = Tex{
                             .name = gltfImage.name.c_str(),
                             .data = nullptr,
                             .size = 0,
                             .format = imageFormat,
                             .dimensions = glm::uvec2(ktx2Texture->baseWidth, ktx2Texture->baseHeight),
                             .ktx2Texture = ktx2Texture,
                         };
                       },
                       [&](const fastgltf::sources::Vector& vector) -> void { fromMemory(vector.bytes); },
                       [&](const fastgltf::sources::BufferView view) -> void {
                         auto& bv = bufferViews[view.bufferViewIndex];
                         auto& buf = buffers[bv.bufferIndex];

                         std::visit(fastgltf::visitor{
                                        [](auto& arg) -> void { KT_ABORT("Unsupported buffer source type: {}", typeid(arg).name()); },
                                        [&](const fastgltf::sources::Array& array) -> void {
                                          fromMemory(std::span<const std::byte>(array.bytes.data() + bv.byteOffset, bv.byteLength));
                                        },
                                        [&](const fastgltf::sources::Vector& vector) -> void {
                                          fromMemory(std::span<const std::byte>(vector.bytes.data() + bv.byteOffset, bv.byteLength));
                                        },
                                    },
                                    buf.data);
                       },
                   },
                   gltfImage.data);
      }
    });

    auto& rhi = rhi::RHI::get();

    std::vector<rhi::ImageRef> resultTextures;
    resultTextures.reserve(texturesToLoad.size());
    uint64_t copyValue = rhi.oneshotCopy([&](rhi::CommandBuffer& cmd) {
      std::vector<rhi::Buffer> stagingBuffers;
      stagingBuffers.reserve(texturesToLoad.size());

      for (const auto& tex : texturesToLoad) {
        auto imageRes = rhi.createTexture({rhi::ImageDim::e2D,
                                           tex.format,
                                           {tex.dimensions.x, tex.dimensions.y, 1},
                                           rhi::ImageUsage::Sampled | rhi::ImageUsage::TransferDst,
                                           tex.ktx2Texture != nullptr ? tex.ktx2Texture->numLevels : 1,
                                           1,
                                           tex.name});
        KT_ASSERT(imageRes.isOk(), "Failed to create image: {}", imageRes.error());
        auto& image = imageRes.value();
        resultTextures.emplace_back(image);

        if (tex.ktx2Texture != nullptr) {
          auto stagingRes = rhi::Buffer::create(
              {tex.ktx2Texture->dataSize, rhi::BufferUsage::TransferSrc, rhi::BufferType::Staging, "GLTF KTX2 Staging Buffer"});
          KT_ASSERT(stagingRes.isOk(), "Failed to create staging buffer for KTX2 image: {}", stagingRes.error());
          stagingBuffers.emplace_back(std::move(stagingRes.value()));
          auto& staging = stagingBuffers.back();

          std::memcpy(staging.mapping<void>(0), tex.ktx2Texture->pData, tex.ktx2Texture->dataSize);

          cmd.transitionImage(image, rhi::ImageLayout::Undefined, rhi::ImageLayout::TransferDst);
          for (ktx_uint32_t level = 0; level < tex.ktx2Texture->numLevels; ++level) {
            size_t offset = 0;
            ktxTexture2_GetImageOffset(tex.ktx2Texture, level, 0, 0, &offset);

            cmd.copyBufferToImage(staging, image, tex.dimensions.x >> level, tex.dimensions.y >> level, level, offset);
          }
          cmd.transitionImage(image, rhi::ImageLayout::TransferDst, rhi::ImageLayout::ShaderReadOnly);

          ktxTexture2_Destroy(tex.ktx2Texture);
        } else {
          auto stagingRes = rhi::Buffer::create({tex.size, rhi::BufferUsage::TransferSrc, rhi::BufferType::Staging, "GLTF Staging Buffer"});
          KT_ASSERT(stagingRes.isOk(), "Failed to create staging buffer for image: {}", stagingRes.error());
          stagingBuffers.emplace_back(std::move(stagingRes.value()));
          auto& staging = stagingBuffers.back();

          std::memcpy(staging.mapping<void>(0), tex.data, tex.size);

          cmd.transitionImage(image, rhi::ImageLayout::Undefined, rhi::ImageLayout::TransferDst);
          cmd.copyBufferToImage(staging, image, tex.dimensions.x, tex.dimensions.y);
          cmd.transitionImage(image, rhi::ImageLayout::TransferDst, rhi::ImageLayout::ShaderReadOnly);

          stbi_image_free(tex.data);
        }
      }

      return stagingBuffers;
    });

    return Data::PartialUploadResult<std::vector<rhi::ImageRef>>{
        .copyFenceValue = copyValue,
        .result = std::move(resultTextures),
    };
  }

  MeshSize MeshData::getSize() const {
    return MeshSize{.positions = sizeof(glm::vec3) * positions.size(),
                    .vertexAttribs = sizeof(VertexAttribs) * vertexAttribs.size(),
                    .indices = sizeof(uint32_t) * indices.size(),
                    .submeshes = sizeof(GpuSubmesh) * submeshes.size(),
                    .meshlets = sizeof(Meshlet) * meshlets.size(),
                    .meshletVertices = sizeof(uint32_t) * meshletVertices.size(),
                    .meshletTriangles = sizeof(uint32_t) * meshletTriangles.size()};
  }
} // namespace kt::gltf
