#include "keptech/renderer.hpp"

#include <cstdint>
#include <execution>
#include <iterator>
#include <keptech/core/image.hpp>
#include <keptech/core/kt-logger.hpp>
#include <keptech/core/rendering/gltf/data.hpp>
#include <keptech/core/rendering/gltf/scene.hpp>
#include <ranges>

namespace keptech {
  std::expected<Mesh, std::string> Renderer::loadMesh(const MeshData& data) {
    if (data.vertices.empty() || data.indices.empty()) {
      return std::unexpected("Mesh data contains no vertices or indices.");
    }
    auto cmdBufRes = m.backend->createCmdBuffer(CmdBufType::Compute);
    if (!cmdBufRes) {
      return std::unexpected(
          fmt::format("Failed to create command buffer for mesh loading: {}",
                      cmdBufRes.error()));
    }
    CmdBufPtr cmdBuf = std::move(cmdBufRes.value());
    cmdBuf->begin();

    std::vector<BufPtr> trackedBuffers{m.buffers.vertex, m.buffers.index};

    KT_DEBUG("Loading mesh '{}' with {} vertices and {} indices", data.name,
             data.vertices.size(), data.indices.size());

    size_t vertexSize = data.vertices.size() * sizeof(Vertex);
    size_t indexSize = data.indices.size() * sizeof(uint32_t);

    size_t requiredVertexBufferSize = m.buffers.vertexEnd + vertexSize;
    size_t requiredIndexBufferSize = m.buffers.indexEnd + indexSize;

    if (requiredVertexBufferSize > m.buffers.vertex->getSize()) {

      KT_DEBUG("Reallocating vertex buffer from {} bytes to {} bytes to fit "
               "glTF data",
               m.buffers.vertex->getSize(), requiredVertexBufferSize);

      auto res = m.backend->createBuffer({
          .name = "Vertex Buffer",
          .size = requiredVertexBufferSize,
          .usage = BufferUsage::Vertex | BufferUsage::TransferDst |
                   BufferUsage::TransferSrc,
          .memoryType = BufferMemoryType::PreferDevice,
          .map = BufferMapType::SeqWrite,
      });
      if (!res) {
        return std::unexpected(
            fmt::format("Failed to upsize vertex buffer for glTF loading: {}",
                        res.error()));
      }
      if (m.buffers.vertexEnd > 0) {
        cmdBuf->copyBufferToBuffer(*m.buffers.vertex.get(), *res.value().get(),
                                   m.buffers.vertexEnd, 0, 0);
        trackedBuffers.push_back(res.value());
      }
      m.buffers.vertex = std::move(res.value());
    }

    if (requiredIndexBufferSize > m.buffers.index->getSize()) {
      KT_DEBUG("Reallocating index buffer from {} bytes to {} bytes to fit "
               "glTF data",
               m.buffers.index->getSize(), requiredIndexBufferSize);

      auto res = m.backend->createBuffer({
          .name = "Index Buffer",
          .size = requiredIndexBufferSize,
          .usage = BufferUsage::Index | BufferUsage::TransferDst |
                   BufferUsage::TransferSrc,
          .memoryType = BufferMemoryType::PreferDevice,
          .map = BufferMapType::SeqWrite,
      });
      if (!res) {
        return std::unexpected(fmt::format(
            "Failed to upsize index buffer for glTF loading: {}", res.error()));
      }
      if (m.buffers.indexEnd > 0) {
        cmdBuf->copyBufferToBuffer(*m.buffers.index.get(), *res.value().get(),
                                   m.buffers.indexEnd, 0, 0);
        trackedBuffers.push_back(res.value());
      }
      m.buffers.index = std::move(res.value());
    }

    BufPtr vertexBuf = m.buffers.vertex;
    BufPtr indexBuf = m.buffers.index;

    size_t vertexOffset = m.buffers.vertexEnd;
    size_t indexOffset = m.buffers.indexEnd;

    bool needCopy = m.buffers.vertex->getMapping() == nullptr ||
                    m.buffers.index->getMapping() == nullptr;

    if (needCopy) {
      size_t totalSize = vertexSize + indexSize;
      BufferCreateInfo stagingBufInfo{
          .name = fmt::format("Staging Buffer for Mesh '{}'", data.name),
          .size = totalSize,
          .usage = BufferUsage::TransferSrc,
          .map = BufferMapType::SeqWrite,
      };
      auto stagingBufRes = m.backend->createBuffer(stagingBufInfo);
      if (!stagingBufRes) {
        return std::unexpected(
            fmt::format("Failed to create staging buffer for mesh loading: {}",
                        stagingBufRes.error()));
      }
      vertexBuf = std::move(stagingBufRes.value());
      indexBuf = vertexBuf;
      trackedBuffers.push_back(vertexBuf);
      vertexOffset = 0;
      indexOffset = vertexSize;
    }

    uint8_t* vertexMapping =
        static_cast<uint8_t*>(vertexBuf->getMapping()) + vertexOffset;
    uint8_t* indexMapping =
        static_cast<uint8_t*>(indexBuf->getMapping()) + indexOffset;

    memcpy(vertexMapping, data.vertices.data(), vertexSize);
    memcpy(indexMapping, data.indices.data(), indexSize);

    if (needCopy) {
      cmdBuf->copyBufferToBuffer(*vertexBuf.get(), *m.buffers.vertex.get(),
                                 vertexSize, vertexOffset, m.buffers.vertexEnd);
      cmdBuf->copyBufferToBuffer(*indexBuf.get(), *m.buffers.index.get(),
                                 indexSize, indexOffset, m.buffers.indexEnd);
    }
    cmdBuf->end();

    std::vector<IRendererBackend::SubmitInfo> submitInfos;
    submitInfos.push_back(IRendererBackend::SubmitInfo{
        .commandBuffer = std::move(cmdBuf),
        .trackedBuffers = std::move(trackedBuffers),
    });
    m.backend->submitCommandBuffers(std::move(submitInfos));

    Mesh mesh(m.buffers.vertexEnd / sizeof(Vertex), data.indices.size(),
              m.buffers.indexEnd / sizeof(uint32_t)
#ifdef KT_ADD_RESOURCE_INFO
                  ,
              data.name, data.vertices.size()
#endif
    );

    KT_DEBUG("Created mesh at offset {}", mesh.getVertexOffset());

    m.buffers.vertexEnd += vertexSize;
    m.buffers.indexEnd += indexSize;

    return mesh;
  }

  namespace {
    struct Submesh {
      MeshPtr mesh;
      uint32_t materialIndex;
    };

    gltf::Scene::Node
    processNode(const gltf::Data::Node& node,
                const std::vector<std::vector<Submesh>>& meshes,
                const std::vector<MaterialPtr>& materials,
                const std::vector<fastgltf::Light>& lights) {
      gltf::Scene::Node sceneNode{
          .name = std::string(node.node.name),
          .transform = node.transform,
          .mesh = nullptr,
          .material = nullptr,
      };

      if (node.meshIndex != UINT32_MAX) {
        auto& submeshes = meshes[node.meshIndex];

        if (submeshes.size() == 1) {
          sceneNode.mesh = submeshes[0].mesh;
          sceneNode.material = materials[submeshes[0].materialIndex];
        } else {
          for (auto& submesh : submeshes) {
            sceneNode.children.push_back(gltf::Scene::Node{
#ifdef KT_ADD_RESOURCE_INFO
                .name = submesh.mesh->getDebugName(),
#endif
                .transform = maths::Transform{},
                .mesh = submesh.mesh,
                .material = materials[submesh.materialIndex]});
          }
        }
      }

      if (node.lightIndex != ~0u) {
        auto& color = lights[node.lightIndex].color;
        KT_INFO("Node '{}' has a point light with color ({}, {}, {}) and "
                "intensity {}",
                sceneNode.name, color.x(), color.y(), color.z(),
                lights[node.lightIndex].intensity);

        sceneNode.pointLight = components::PointLight{
            .color = {color.x(), color.y(), color.z()},
            .intensity = lights[node.lightIndex].intensity,
        };
      }

      for (auto& child : node.children) {
        sceneNode.children.push_back(
            processNode(child, meshes, materials, lights));
      }

      return sceneNode;
    }
  } // namespace

  std::expected<gltf::Scene, std::string>
  Renderer::loadMesh(std::string_view path) {
    auto cmdBufRes = m.backend->createCmdBuffer(CmdBufType::Compute);
    if (!cmdBufRes) {
      return std::unexpected(
          fmt::format("Failed to create command buffer for mesh loading: {}",
                      cmdBufRes.error()));
    }
    CmdBufPtr cmdBuf = std::move(cmdBufRes.value());
    cmdBuf->begin();

    auto res = gltf::Data::fromFile(path);
    if (!res) {
      return std::unexpected(
          fmt::format("Failed to load glTF file '{}': {}", path, res.error()));
    }
    auto& gltf = res.value();

    size_t requiredVertexBufferSize = 0;
    size_t requiredIndexBufferSize = 0;

    for (auto& meshData : gltf.meshes) {
      requiredVertexBufferSize += meshData.vertices.size() * sizeof(Vertex);
      requiredIndexBufferSize += meshData.indices.size() * sizeof(uint32_t);
    }

    size_t requiredMaterialBufferSize =
        gltf.materials.size() * sizeof(DeferredMaterialData);

    KT_DEBUG("Loading glTF '{}' with {} meshes. Vertices: {}, Indices: {}, "
             "Materials: {}",
             path, gltf.meshes.size(), requiredVertexBufferSize,
             requiredIndexBufferSize, requiredMaterialBufferSize);

    std::vector<BufPtr> trackedBuffers{
        m.buffers.vertex,
        m.buffers.index,
        m.buffers.material,
    };

    if (m.buffers.vertexEnd + requiredVertexBufferSize >
        m.buffers.vertex->getSize()) {

      auto newVertexBufferSize =
          m.buffers.vertex->getSize() + requiredVertexBufferSize;

      KT_DEBUG("Reallocating vertex buffer from {} bytes to {} bytes to fit "
               "glTF data",
               m.buffers.vertex->getSize(), newVertexBufferSize);

      auto res = m.backend->createBuffer({
          .name = "Vertex Buffer",
          .size = newVertexBufferSize,
          .usage = BufferUsage::Vertex | BufferUsage::TransferDst |
                   BufferUsage::TransferSrc,
          .memoryType = BufferMemoryType::PreferDevice,
          .map = BufferMapType::SeqWrite,
      });
      if (!res) {
        return std::unexpected(
            fmt::format("Failed to upsize vertex buffer for glTF loading: {}",
                        res.error()));
      }
      if (m.buffers.vertexEnd > 0) {
        cmdBuf->copyBufferToBuffer(*m.buffers.vertex.get(), *res.value().get(),
                                   m.buffers.vertexEnd, 0, 0);
        trackedBuffers.push_back(res.value());
      }
      m.buffers.vertex = std::move(res.value());
    }

    if (m.buffers.indexEnd + requiredIndexBufferSize >
        m.buffers.index->getSize()) {
      auto newIndexBufferSize =
          m.buffers.index->getSize() + requiredIndexBufferSize;

      KT_DEBUG("Reallocating index buffer from {} bytes to {} bytes to fit "
               "glTF data",
               m.buffers.index->getSize(), newIndexBufferSize);

      auto res = m.backend->createBuffer({
          .name = "Index Buffer",
          .size = newIndexBufferSize,
          .usage = BufferUsage::Index | BufferUsage::TransferDst |
                   BufferUsage::TransferSrc,
          .memoryType = BufferMemoryType::PreferDevice,
          .map = BufferMapType::SeqWrite,
      });
      if (!res) {
        return std::unexpected(fmt::format(
            "Failed to upsize index buffer for glTF loading: {}", res.error()));
      }
      if (m.buffers.indexEnd > 0) {
        cmdBuf->copyBufferToBuffer(*m.buffers.index.get(), *res.value().get(),
                                   m.buffers.indexEnd, 0, 0);
        trackedBuffers.push_back(res.value());
      }
      m.buffers.index = std::move(res.value());
    }

    if (m.buffers.materialEnd + requiredMaterialBufferSize >
        m.buffers.material->getSize()) {
      auto newMaterialBufferSize =
          m.buffers.material->getSize() + requiredMaterialBufferSize;

      KT_DEBUG("Reallocating material buffer from {} bytes to {} bytes to fit "
               "glTF data",
               m.buffers.material->getSize(), newMaterialBufferSize);

      auto res = m.backend->createBuffer({
          .name = "Material Buffer",
          .size = newMaterialBufferSize,
          .usage = BufferUsage::Uniform | BufferUsage::TransferDst |
                   BufferUsage::TransferSrc,
          .memoryType = BufferMemoryType::PreferDevice,
          .map = BufferMapType::SeqWrite,
      });
      if (!res) {
        return std::unexpected(
            fmt::format("Failed to upsize material buffer for glTF loading: {}",
                        res.error()));
      }
      if (m.buffers.materialEnd > 0) {
        cmdBuf->copyBufferToBuffer(*m.buffers.material.get(),
                                   *res.value().get(), m.buffers.materialEnd, 0,
                                   0);
        trackedBuffers.push_back(res.value());
      }
      m.buffers.material = std::move(res.value());
    }

    BufPtr vertexBuf = m.buffers.vertex;
    BufPtr indexBuf = m.buffers.index;
    BufPtr materialBuf = m.buffers.material;

    bool needCopyVertex = vertexBuf->getMapping() == nullptr;
    bool needCopyIndex = indexBuf->getMapping() == nullptr;
    bool needCopyMaterial = materialBuf->getMapping() == nullptr;

    size_t vertexOffset = m.buffers.vertexEnd;
    size_t indexOffset = m.buffers.indexEnd;
    size_t materialOffset = m.buffers.materialEnd;

    size_t totalSize = 0;
    if (needCopyVertex) {
      vertexOffset = 0;
      totalSize += requiredVertexBufferSize;
    }
    if (needCopyIndex) {
      indexOffset = needCopyVertex ? requiredVertexBufferSize : 0;
      totalSize += requiredIndexBufferSize;
    }
    if (needCopyMaterial) {
      materialOffset =
          indexOffset + (needCopyIndex ? requiredIndexBufferSize : 0);
      totalSize += requiredMaterialBufferSize;
    }

    if (totalSize > 0) {
      BufferCreateInfo stagingBufInfo{
          .name = fmt::format("Staging Buffer for glTF '{}'", path),
          .size = totalSize,
          .usage = BufferUsage::TransferSrc,
          .map = BufferMapType::SeqWrite,
      };
      auto stagingBufRes = m.backend->createBuffer(stagingBufInfo);
      if (!stagingBufRes) {
        return std::unexpected(
            fmt::format("Failed to create staging buffer for glTF loading: {}",
                        stagingBufRes.error()));
      }
      auto stagingBufPtr = std::move(stagingBufRes.value());

      trackedBuffers.push_back(stagingBufPtr);
      if (needCopyVertex) {
        vertexBuf = stagingBufPtr;
      }
      if (needCopyIndex) {
        indexBuf = stagingBufPtr;
      }
      if (needCopyMaterial) {
        materialBuf = stagingBufPtr;
      }
    }

    std::vector<std::vector<Submesh>> meshes;

    size_t startVertexEnd = m.buffers.vertexEnd;
    size_t startIndexEnd = m.buffers.indexEnd;

    std::vector<MeshPtr> allMeshes;

    uint8_t* vertexMapping =
        static_cast<uint8_t*>(vertexBuf->getMapping()) + vertexOffset;
    uint8_t* indexMapping =
        static_cast<uint8_t*>(indexBuf->getMapping()) + indexOffset;
    uint8_t* materialMapping =
        static_cast<uint8_t*>(materialBuf->getMapping()) + materialOffset;
    for (auto& meshData : gltf.meshes) {
      size_t vertexSize = meshData.vertices.size() * sizeof(Vertex);
      size_t indexSize = meshData.indices.size() * sizeof(uint32_t);

      memcpy(vertexMapping, meshData.vertices.data(), vertexSize);
      vertexMapping += vertexSize;
      memcpy(indexMapping, meshData.indices.data(), indexSize);
      indexMapping += indexSize;

      std::vector<Submesh> submeshes;
      submeshes.reserve(meshData.submeshes.size());
      size_t i = 0;
      uint32_t vEnd = m.buffers.vertexEnd / sizeof(Vertex);
      uint32_t iEnd = m.buffers.indexEnd / sizeof(uint32_t);
      for (auto& submeshData : meshData.submeshes) {
        MeshPtr submesh = std::make_shared<Mesh>(
            vEnd, submeshData.indexCount, iEnd + submeshData.indexOffset
#ifdef KT_ADD_RESOURCE_INFO
            ,
            meshData.submeshes.size() == 1
                ? meshData.name
                : fmt::format("{} submesh {}", meshData.name, i),
            meshData.vertices.size()
#endif
        );
        allMeshes.emplace_back(submesh);
        submeshes.emplace_back(std::move(submesh), submeshData.materialIndex);
        ++i;
      }

      m.buffers.vertexEnd += vertexSize;
      m.buffers.indexEnd += indexSize;

      meshes.emplace_back(std::move(submeshes));
    }

    if (needCopyVertex) {
      cmdBuf->copyBufferToBuffer(*vertexBuf.get(), *m.buffers.vertex.get(),
                                 requiredVertexBufferSize, vertexOffset,
                                 startVertexEnd);
    }
    if (needCopyIndex) {
      cmdBuf->copyBufferToBuffer(*indexBuf.get(), *m.buffers.index.get(),
                                 requiredIndexBufferSize, indexOffset,
                                 startIndexEnd);
    }

    std::vector<Image> imageData;
    imageData.resize(gltf.images.size());

    std::vector<IRendererBackend::ImageUploadInfo> imageUploadInfos;
    imageUploadInfos.resize(gltf.images.size());

    auto enumView = std::views::enumerate(gltf.images);

    auto visitor = fastgltf::visitor{
        [](auto& arg) -> std::expected<keptech::Image, std::string> {
          KT_ERROR("Unsupported glTF image source type {}", typeid(arg).name());
          return std::unexpected("Unsupported glTF image source.");
        },
        [&](fastgltf::sources::Array& array) {
          return keptech::Image::loadFromMemory(
              reinterpret_cast<const uint8_t*>(array.bytes.data()),
              static_cast<int>(array.bytes.size()), 4);
        },
        [&](fastgltf::sources::URI& filePath) {
          assert(filePath.fileByteOffset ==
                 0); // We don't support offsets with stbi.
          assert(filePath.uri.isLocalPath()); // We're only capable of
                                              // loading local files.

          const std::string path(filePath.uri.path().begin(),
                                 filePath.uri.path().end());
          return keptech::Image::loadFromFile(path.c_str(), 4);
        },
        [&](fastgltf::sources::Vector& vector) {
          return keptech::Image::loadFromMemory(
              reinterpret_cast<const uint8_t*>(vector.bytes.data()),
              static_cast<int>(vector.bytes.size()), 4);
        },
        [&](fastgltf::sources::BufferView view) {
          auto& bufferView = gltf.bufferViews[view.bufferViewIndex];
          auto& buffer = gltf.buffers[bufferView.bufferIndex];

          return std::visit(
              fastgltf::visitor{
                  // We only care about VectorWithMime here, because
                  // we
                  // specify LoadExternalBuffers, meaning all buffers
                  // are already loaded into a vector.
                  [](auto& arg) -> std::expected<keptech::Image, std::string> {
                    KT_ERROR("Unsupported glTF image source buffer "
                             "type {}",
                             typeid(arg).name());
                    return std::unexpected("Unsupported glTF image "
                                           "source in buffer view.");
                  },
                  [&](fastgltf::sources::Array& array) {
                    return keptech::Image::loadFromMemory(
                        reinterpret_cast<uint8_t*>(array.bytes.data()) +
                            bufferView.byteOffset,
                        static_cast<int>(bufferView.byteLength), 4);
                  },
                  [&](fastgltf::sources::Vector& vector) {
                    return keptech::Image::loadFromMemory(
                        reinterpret_cast<uint8_t*>(vector.bytes.data()) +
                            bufferView.byteOffset,
                        static_cast<int>(bufferView.byteLength), 4);
                  },
              },
              buffer.data);
        },
    };

    std::for_each(std::execution::par, enumView.begin(), enumView.end(),
                  [&](const std::tuple<size_t, fastgltf::Image&>& pair) {
                    const auto& [idx, img] = pair;

                    auto imgRes = std::visit(visitor, img.data);

                    KT_ASSERT(imgRes, "Failed to load glTF image '{}': {}",
                              img.name, imgRes.error());

                    imageData[idx] = std::move(imgRes.value());
                    imageUploadInfos[idx] = IRendererBackend::ImageUploadInfo{
                        .name = std::string(img.name),
                        .image = &imageData[idx],
                        .usage =
                            TextureUsage::Sampled | TextureUsage::TransferDst};
                  });

    auto imagesRes = m.backend->createImages(imageUploadInfos);
    if (!imagesRes) {
      return std::unexpected(
          fmt::format("Failed to create glTF images: {}", imagesRes.error()));
    }
    imageData.clear(); // Free image data memory after upload.

    std::vector<ImgPtr> textures = std::move(imagesRes.value());

    std::vector<MaterialPtr> materials;
    size_t startMaterialOffset = m.buffers.materialEnd;
    for (auto& matData : gltf.materials) {
      struct Data {
        glm::vec4 albedoColor{matData.pbrData.baseColorFactor.x(),
                              matData.pbrData.baseColorFactor.y(),
                              matData.pbrData.baseColorFactor.z(),
                              matData.pbrData.baseColorFactor.w()};
        ImgPtr albedoTex;
        glm::vec2 albedoScale{1.f, 1.f};
        glm::vec2 albedoOffset;
        float albedoRotation;
        ImgPtr normalTex;
        glm::vec2 normalScale{1.f, 1.f};
        glm::vec2 normalOffset;
        float normalRotation;
        glm::vec3 emissiveColor{matData.emissiveFactor.x(),
                                matData.emissiveFactor.y(),
                                matData.emissiveFactor.z()};
        ImgPtr emissiveTex;
        glm::vec2 emissiveScale{1.f, 1.f};
        glm::vec2 emissiveOffset;
        float emissiveRotation;
        float metallic = matData.pbrData.metallicFactor;
        float roughness = matData.pbrData.roughnessFactor;
        ImgPtr metallicRoughnessTex;
        glm::vec2 metallicRoughnessScale{1.f, 1.f};
        glm::vec2 metallicRoughnessOffset;
        float metallicRoughnessRotation;
        ImgPtr ao;
        glm::vec2 aoScale{1.f, 1.f};
        glm::vec2 aoOffset;
        float aoRotation;
      } data{};

      if (matData.pbrData.baseColorTexture.has_value()) {
        auto& texInfo = matData.pbrData.baseColorTexture.value();

        if (texInfo.transform) {
          data.albedoScale = {texInfo.transform->uvScale.x(),
                              texInfo.transform->uvScale.y()};
          data.albedoOffset = {texInfo.transform->uvOffset.x(),
                               texInfo.transform->uvOffset.y()};
          data.albedoRotation = texInfo.transform->rotation;
        }

        data.albedoTex =
            textures[gltf.textures[texInfo.textureIndex].imageIndex.value_or(
                0)];
      }
      if (matData.normalTexture.has_value()) {
        auto& texInfo = matData.normalTexture.value();

        if (texInfo.transform) {
          data.normalScale = {texInfo.transform->uvScale.x(),
                              texInfo.transform->uvScale.y()};
          data.normalOffset = {texInfo.transform->uvOffset.x(),
                               texInfo.transform->uvOffset.y()};
          data.normalRotation = texInfo.transform->rotation;
        }

        data.normalTex =
            textures[gltf.textures[texInfo.textureIndex].imageIndex.value_or(
                0)];
      }

      if (matData.emissiveTexture.has_value()) {
        auto& texInfo = matData.emissiveTexture.value();

        if (texInfo.transform) {
          data.emissiveScale = {texInfo.transform->uvScale.x(),
                                texInfo.transform->uvScale.y()};
          data.emissiveOffset = {texInfo.transform->uvOffset.x(),
                                 texInfo.transform->uvOffset.y()};
          data.emissiveRotation = texInfo.transform->rotation;
        }

        data.emissiveTex =
            textures[gltf.textures[texInfo.textureIndex].imageIndex.value_or(
                0)];
      }

      if (matData.pbrData.metallicRoughnessTexture.has_value()) {
        auto& texInfo = matData.pbrData.metallicRoughnessTexture.value();

        if (texInfo.transform) {
          data.metallicRoughnessScale = {texInfo.transform->uvScale.x(),
                                         texInfo.transform->uvScale.y()};
          data.metallicRoughnessOffset = {texInfo.transform->uvOffset.x(),
                                          texInfo.transform->uvOffset.y()};
          data.metallicRoughnessRotation = texInfo.transform->rotation;
        }

        data.metallicRoughnessTex =
            textures[gltf.textures[texInfo.textureIndex].imageIndex.value_or(
                0)];
      }

      if (matData.occlusionTexture.has_value()) {
        auto& texInfo = matData.occlusionTexture.value();

        if (texInfo.transform) {
          data.aoScale = {texInfo.transform->uvScale.x(),
                          texInfo.transform->uvScale.y()};
          data.aoOffset = {texInfo.transform->uvOffset.x(),
                           texInfo.transform->uvOffset.y()};
          data.aoRotation = texInfo.transform->rotation;
        }

        data.ao =
            textures[gltf.textures[texInfo.textureIndex].imageIndex.value_or(
                0)];
      }

      DeferredMaterialData gpuData{
          .albedoColor = data.albedoColor,
          .emissiveColor = data.emissiveColor,
          .metallic = data.metallic,
          .albedo =
              {
                  .uvScale = data.albedoScale,
                  .uvOffset = data.albedoOffset,
                  .rotation = data.albedoRotation,
                  .texIndex = data.albedoTex ? data.albedoTex->getIndex() : ~0u,
              },
          .bump =
              {
                  .uvScale = data.normalScale,
                  .uvOffset = data.normalOffset,
                  .rotation = data.normalRotation,
                  .texIndex = data.normalTex ? data.normalTex->getIndex() : ~0u,
              },
          .emissive =
              {
                  .uvScale = data.emissiveScale,
                  .uvOffset = data.emissiveOffset,
                  .rotation = data.emissiveRotation,
                  .texIndex =
                      data.emissiveTex ? data.emissiveTex->getIndex() : ~0u,
              },
          .metallicRoughness =
              {
                  .uvScale = data.metallicRoughnessScale,
                  .uvOffset = data.metallicRoughnessOffset,
                  .rotation = data.metallicRoughnessRotation,
                  .texIndex = data.metallicRoughnessTex
                                  ? data.metallicRoughnessTex->getIndex()
                                  : ~0u,
              },
          .ao =
              {
                  .uvScale = data.aoScale,
                  .uvOffset = data.aoOffset,
                  .rotation = data.aoRotation,
                  .texIndex = data.ao ? data.ao->getIndex() : ~0u,
              },
          .roughness = data.roughness,
      };

      memcpy(materialMapping, &gpuData, sizeof(DeferredMaterialData));
      materialMapping += sizeof(DeferredMaterialData);

      PipelinePtr& pipeline = m.pipelines.deferred;

      MaterialPtr material =
          std::make_shared<Material>(pipeline,
                                     std::vector<keptech::MaterialData>{
                                         data.albedoColor,
                                         data.emissiveColor,
                                         data.metallic,
                                         data.albedoScale,
                                         data.albedoOffset,
                                         data.albedoRotation,
                                         data.albedoTex,
                                         data.normalScale,
                                         data.normalOffset,
                                         data.normalRotation,
                                         data.normalTex,
                                         data.metallicRoughnessScale,
                                         data.metallicRoughnessOffset,
                                         data.metallicRoughnessRotation,
                                         data.metallicRoughnessTex,
                                         data.emissiveScale,
                                         data.emissiveOffset,
                                         data.emissiveRotation,
                                         data.emissiveTex,
                                         data.aoScale,
                                         data.aoOffset,
                                         data.aoRotation,
                                         data.ao,
                                         data.roughness,
                                     },
                                     m.buffers.materialEnd);

      m.buffers.materialEnd += sizeof(DeferredMaterialData);
      materials.emplace_back(std::move(material));
    }

    if (materials.empty()) {
      KT_WARN("No materials found in glTF '{}', using default material", path);
      materials.push_back(m.defaultMaterial);
    }

    if (needCopyMaterial && requiredMaterialBufferSize > 0) {
      cmdBuf->copyBufferToBuffer(*materialBuf.get(), *m.buffers.material.get(),
                                 requiredMaterialBufferSize, materialOffset,
                                 startMaterialOffset);
    }

    cmdBuf->end();

    std::vector<IRendererBackend::SubmitInfo> submitInfos;
    submitInfos.push_back(IRendererBackend::SubmitInfo{
        .commandBuffer = std::move(cmdBuf),
        .trackedBuffers = std::move(trackedBuffers),
    });
    m.backend->submitCommandBuffers(std::move(submitInfos));

    std::vector<gltf::Scene::Node> sceneNodes;
    sceneNodes.reserve(gltf.roots.size());

    for (auto& node : gltf.roots) {
      sceneNodes.push_back(processNode(node, meshes, materials, gltf.lights));
    }

    gltf::Scene scene{
        .roots = std::move(sceneNodes),
        .meshes = allMeshes,
    };

    return std::move(scene);
  }
} // namespace keptech
