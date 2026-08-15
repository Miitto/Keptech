#include "data.hpp"

#include "keptech/rhi/bufferUsage.hpp"
#include "keptech/rhi/rhi.hpp"

#include "buffers.hpp"
#include "keptech/core/kt-logger.hpp"
#include "keptech/rhi/bufferCreateInfo.hpp"
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
    auto& buffers = kt::Buffers::get();
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

    std::optional<rhi::Buffer> oldPosBuf;
    std::optional<rhi::Buffer> oldVertexAttribsBuf;
    std::optional<rhi::Buffer> oldIndicesBuf;
    std::optional<rhi::Buffer> oldSubmeshesBuf;
    std::optional<rhi::Buffer> oldMeshletsBuf;
    std::optional<rhi::Buffer> oldMeshletVerticesBuf;
    std::optional<rhi::Buffer> oldMeshletTrianglesBuf;

    if (!buffers.positions.hasSpaceFor(counts.positions)) {
      auto newPosBufRes =
          rhi::Buffer::create({buffers.positions.occupied() + totalSize.positions, rhi::BufferUsage::Vertex | rhi::BufferUsage::TransferDst,
                               rhi::BufferType::Default, "GLTF Positions Buffer"});
      KT_ASSERT(newPosBufRes.isOk(), "Failed to reallocate positions buffer: {}", newPosBufRes.error());
      if (buffers.positions->isValid())
        oldPosBuf = std::move(buffers.positions.getBuffer());
      buffers.positions.getBuffer() = std::move(newPosBufRes.value());
    }

    if (!buffers.vertexAttribs.hasSpaceFor(counts.vertexAttribs)) {
      auto newVertexAttribsBufRes = rhi::Buffer::create({buffers.vertexAttribs.occupied() + totalSize.vertexAttribs,
                                                         rhi::BufferUsage::Vertex | rhi::BufferUsage::TransferDst, rhi::BufferType::Default,
                                                         "GLTF Vertex Attributes Buffer"});
      KT_ASSERT(newVertexAttribsBufRes.isOk(), "Failed to reallocate vertex attributes buffer: {}", newVertexAttribsBufRes.error());
      if (buffers.vertexAttribs->isValid())
        oldVertexAttribsBuf = std::move(buffers.vertexAttribs.getBuffer());
      buffers.vertexAttribs.getBuffer() = std::move(newVertexAttribsBufRes.value());
    }

    if (!buffers.indices.hasSpaceFor(counts.indices)) {
      auto newIndicesBufRes =
          rhi::Buffer::create({buffers.indices.occupied() + totalSize.indices, rhi::BufferUsage::Index | rhi::BufferUsage::TransferDst,
                               rhi::BufferType::Default, "GLTF Indices Buffer"});
      KT_ASSERT(newIndicesBufRes.isOk(), "Failed to reallocate indices buffer: {}", newIndicesBufRes.error());
      if (buffers.indices->isValid())
        oldIndicesBuf = std::move(buffers.indices.getBuffer());
      buffers.indices.getBuffer() = std::move(newIndicesBufRes.value());
    }

    if (!buffers.submeshes.hasSpaceFor(counts.submeshes)) {
      auto newSubmeshesBufRes = rhi::Buffer::create({buffers.submeshes.occupied() + totalSize.submeshes,
                                                     rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst, rhi::BufferType::Default,
                                                     "GLTF Submeshes Buffer"});
      KT_ASSERT(newSubmeshesBufRes.isOk(), "Failed to reallocate submeshes buffer: {}", newSubmeshesBufRes.error());
      if (buffers.submeshes->isValid())
        oldSubmeshesBuf = std::move(buffers.submeshes.getBuffer());
      buffers.submeshes.getBuffer() = std::move(newSubmeshesBufRes.value());
    }

    if (!buffers.meshlets.hasSpaceFor(counts.meshlets)) {
      auto newMeshletsBufRes =
          rhi::Buffer::create({buffers.meshlets.occupied() + totalSize.meshlets, rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst,
                               rhi::BufferType::Default, "GLTF Meshlets Buffer"});
      KT_ASSERT(newMeshletsBufRes.isOk(), "Failed to reallocate meshlets buffer: {}", newMeshletsBufRes.error());
      if (buffers.meshlets->isValid())
        oldMeshletsBuf = std::move(buffers.meshlets.getBuffer());
      buffers.meshlets.getBuffer() = std::move(newMeshletsBufRes.value());
    }

    if (!buffers.meshletVertices.hasSpaceFor(counts.meshletVertices)) {
      auto newMeshletVerticesBufRes = rhi::Buffer::create({buffers.meshletVertices.occupied() + totalSize.meshletVertices,
                                                           rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst,
                                                           rhi::BufferType::Default, "GLTF Meshlet Vertices Buffer"});
      KT_ASSERT(newMeshletVerticesBufRes.isOk(), "Failed to reallocate meshlet vertices buffer: {}", newMeshletVerticesBufRes.error());
      if (buffers.meshletVertices->isValid())
        oldMeshletVerticesBuf = std::move(buffers.meshletVertices.getBuffer());
      buffers.meshletVertices.getBuffer() = std::move(newMeshletVerticesBufRes.value());
    }

    if (!buffers.meshletTriangles.hasSpaceFor(counts.meshletTriangles)) {
      auto newMeshletTrianglesBufRes = rhi::Buffer::create({buffers.meshletTriangles.occupied() + totalSize.meshletTriangles,
                                                            rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst,
                                                            rhi::BufferType::Default, "GLTF Meshlet Triangles Buffer"});
      KT_ASSERT(newMeshletTrianglesBufRes.isOk(), "Failed to reallocate meshlet triangles buffer: {}", newMeshletTrianglesBufRes.error());
      if (buffers.meshletTriangles->isValid())
        oldMeshletTrianglesBuf = std::move(buffers.meshletTriangles.getBuffer());
      buffers.meshletTriangles.getBuffer() = std::move(newMeshletTrianglesBufRes.value());
    }

    size_t stagingSize = totalSize.positions + totalSize.vertexAttribs + totalSize.indices + totalSize.submeshes + totalSize.meshlets +
                         totalSize.meshletVertices + totalSize.meshletTriangles;

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
        return gpuSubmesh;
      });
      std::copy(submeshes.begin(), submeshes.end(), submeshesPtr);
      submeshesPtr += submeshes.size();

      std::vector<kt::Submesh> meshSubmeshes(mesh.submeshes.size());
      uint32_t meshId = static_cast<uint32_t>(buffers.submeshes.count());
      std::ranges::transform(mesh.submeshes, meshSubmeshes.begin(), [&](const Submesh& in) {
        kt::Submesh submesh{
            .indexCount = in.index.count,
            .indexOffset = static_cast<uint32_t>(in.index.offset + buffers.vertexAttribs.count()),
            .vertexOffset = static_cast<int32_t>(in.vertex.offset + buffers.positions.count()),
            .meshletOffset = static_cast<uint32_t>(in.meshlet.offset + buffers.meshlets.count()),
            .meshletCount = in.meshlet.count,
            .meshletVertexOffset = static_cast<uint32_t>(in.meshlet.vertexOffset + buffers.meshletVertices.count()),
            .meshletTriangleOffset = static_cast<uint32_t>(in.meshlet.triangleOffset + buffers.meshletTriangles.count()),
            .vertexCount = in.vertex.count,
            .materialIndex = in.materialIndex,
            .boundingSphere = in.boundingSphere,
            .id = meshId++,
        };
        return submesh;
      });

      resultMeshes.emplace_back(mesh.name, std::move(meshSubmeshes));
    }

    auto copyValue = rhi::RHI::get().oneshotCopy([&](rhi::CommandBuffer& cmd) {
      cmd.copyBufferRegion(buffers.positions, staging, buffers.positions.occupied(), positionsOffset, totalSize.positions);
      buffers.positions.registerWrites(counts.positions);
      cmd.copyBufferRegion(buffers.vertexAttribs, staging, buffers.vertexAttribs.occupied(), vertexAttribsOffset, totalSize.vertexAttribs);
      buffers.vertexAttribs.registerWrites(counts.vertexAttribs);
      cmd.copyBufferRegion(buffers.indices, staging, buffers.indices.occupied(), indicesOffset, totalSize.indices);
      buffers.indices.registerWrites(counts.indices);
      cmd.copyBufferRegion(buffers.submeshes, staging, buffers.submeshes.occupied(), submeshesOffset, totalSize.submeshes);
      buffers.submeshes.registerWrites(counts.submeshes);
      cmd.copyBufferRegion(buffers.meshlets, staging, buffers.meshlets.occupied(), meshletsOffset, totalSize.meshlets);
      buffers.meshlets.registerWrites(counts.meshlets);
      cmd.copyBufferRegion(buffers.meshletVertices, staging, buffers.meshletVertices.occupied(), meshletVerticesOffset,
                           totalSize.meshletVertices);
      buffers.meshletVertices.registerWrites(counts.meshletVertices);
      cmd.copyBufferRegion(buffers.meshletTriangles, staging, buffers.meshletTriangles.occupied(), meshletTrianglesOffset,
                           totalSize.meshletTriangles);
      buffers.meshletTriangles.registerWrites(counts.meshletTriangles);

      if (oldPosBuf.has_value()) {
        cmd.copyBufferRegion(buffers.positions, *oldPosBuf, 0, 0, oldPosBuf->size());
        rhi::RHI::get().submitBufferToDrop(*oldPosBuf);
      }
      if (oldVertexAttribsBuf.has_value()) {
        cmd.copyBufferRegion(buffers.vertexAttribs, *oldVertexAttribsBuf, 0, 0, oldVertexAttribsBuf->size());
        rhi::RHI::get().submitBufferToDrop(*oldVertexAttribsBuf);
      }
      if (oldIndicesBuf.has_value()) {
        cmd.copyBufferRegion(buffers.indices, *oldIndicesBuf, 0, 0, oldIndicesBuf->size());
        rhi::RHI::get().submitBufferToDrop(*oldIndicesBuf);
      }
      if (oldSubmeshesBuf.has_value()) {
        cmd.copyBufferRegion(buffers.submeshes, *oldSubmeshesBuf, 0, 0, oldSubmeshesBuf->size());
        rhi::RHI::get().submitBufferToDrop(*oldSubmeshesBuf);
      }
      if (oldMeshletsBuf.has_value()) {
        cmd.copyBufferRegion(buffers.meshlets, *oldMeshletsBuf, 0, 0, oldMeshletsBuf->size());
        rhi::RHI::get().submitBufferToDrop(*oldMeshletsBuf);
      }
      if (oldMeshletVerticesBuf.has_value()) {
        cmd.copyBufferRegion(buffers.meshletVertices, *oldMeshletVerticesBuf, 0, 0, oldMeshletVerticesBuf->size());
        rhi::RHI::get().submitBufferToDrop(*oldMeshletVerticesBuf);
      }
      if (oldMeshletTrianglesBuf.has_value()) {
        cmd.copyBufferRegion(buffers.meshletTriangles, *oldMeshletTrianglesBuf, 0, 0, oldMeshletTrianglesBuf->size());
        rhi::RHI::get().submitBufferToDrop(*oldMeshletTrianglesBuf);
      }
    });

    KT_DEBUG("Uploaded {} meshes to GPU", meshes.size());

    return Data::UploadResult{
        .scene = Scene(*this, resultMeshes, copyValue),
        .copyFenceValue = copyValue,
        .stagingBuffer = std::move(staging),
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
