#include "renderer.hpp"

#include "constants.hpp"
#include "gpuObjects.hpp"
#include "keptech/render/gltf/data.hpp"
#include "keptech/render/gltf/scene.hpp"
#include "loading/mesh.hpp"
#include "macros.hpp"
#include "profile.hpp"
#include "stb/image.h"
#include "vk-logger.hpp"
#include "wrappers/bufferCreateInfo.hpp"
#include "wrappers/imageCreateInfo.hpp"
#include <cstring>
#include <execution>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/components/camera.hpp>
#include <keptech/core/window.hpp>
#include <ktx.h>
#include <ktxvulkan.h>
#include <string_view>

namespace kt::rdr {
  bool Renderer::canRenderToFormat(VkFormat format) const {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(m.vkcore.device, format, &formatProps);
    return ((formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT) |
            (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT)) != 0;
  }

  namespace {
    gltf::Scene::Node createNode(const gltf::Data::Node& node, const std::vector<Mesh>& meshes) {
      gltf::Scene::Node result{
          .name = std::string(node.node.name),
          .transform = node.transform,
          .mesh = node.meshIndex == ~0u ? Mesh() : meshes[node.meshIndex],
      };
      for (auto& child : node.children) {
        result.children.push_back(createNode(child, meshes));
      }

      return result;
    }
  } // namespace

  std::expected<gltf::Scene, std::string> Renderer::loadMesh(std::string_view path) {
    KT_PROFILE_FUNCTION
    VKH_MAKE(gltfData, gltf::Data::fromFile(path), "Failed to load glTF data from file");

    VkCommandBuffer transferCmd = m.vkcore.transferPool.allocate(m.vkcore.device);

    VkCommandBufferBeginInfo cmdBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(transferCmd, &cmdBeginInfo);

    VKH_MAKE(imagesRes, createImages(gltfData, transferCmd), "Failed to create images from glTF data");
    VKH_MAKE(materialsRes, createMaterials(gltfData, imagesRes.resources, transferCmd), "Failed to create materials from glTF data");
    VKH_MAKE(meshesRes, uploadMeshes(gltfData.meshes, materialsRes.resources, transferCmd), "Failed to upload meshes from glTF data");

    vkEndCommandBuffer(transferCmd);

    VkCommandBufferSubmitInfo cmdBufInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = transferCmd,
    };
    VkSubmitInfo2 submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdBufInfo,
    };

    VkFence transferFence = nullptr;
    VkFenceCreateInfo fenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    VK_CHECK(vkCreateFence(m.vkcore.device, &fenceCreateInfo, nullptr, &transferFence), "Failed to create fence for mesh upload");
    vkQueueSubmit2(m.vkcore.queues.transfer.queue, 1, &submitInfo, transferFence);

    VkResult res = VK_SUCCESS;
    {
      KT_PROFILE_SCOPE("Wait for Mesh Upload");
      while (res = vkWaitForFences(m.vkcore.device, 1, &transferFence, VK_TRUE, UINT64_MAX), res == VK_TIMEOUT) {
        std::this_thread::yield();
      }
    }
    VK_ASSERT(res == VK_SUCCESS, "Failed to wait for mesh upload fence");
    vkDestroyFence(m.vkcore.device, transferFence, nullptr);
    vkFreeCommandBuffers(m.vkcore.device, m.vkcore.transferPool.pool, 1, &transferCmd);
    for (auto& buf : meshesRes.stagingBuffers) {
      buf.destroy();
    }
    for (auto& buf : imagesRes.stagingBuffers) {
      buf.destroy();
    }
    for (auto& buf : materialsRes.stagingBuffers) {
      buf.destroy();
    }

    gltf::Scene s{};
    s.roots.reserve(gltfData.roots.size());

    for (const auto& node : gltfData.roots) {
      s.roots.push_back(createNode(node, meshesRes.resources));
    }

    return s;
  }

  std::expected<Renderer::UploadResult<Image>, std::string> Renderer::createImages(const gltf::Data& gltf,
                                                                                   const VkCommandBuffer transferCmd) {
    KT_PROFILE_FUNCTION
    auto& gltfImages = gltf.images;
    if (gltfImages.empty()) {
      return {{.resources = {}, .stagingBuffers = {}}};
    }
    VK_DEBUG("Loading {} images from glTF data", gltfImages.size());
    struct Mip {
      size_t offset;
      uint32_t width;
      uint32_t height;
    };

    struct Tex {
      std::string name;
      ktxTexture2* ktx = nullptr;
    };

    std::vector<Tex> textures(gltfImages.size());
    constexpr ktxTextureCreateFlags createFlags = KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT;

    auto isKtxFile = [](const std::span<const std::byte>& bytes) {
      return bytes.size() >= 12 && memcmp(bytes.data(), "\xABKTX 20\xBB\r\n\x1A\n", 12) == 0;
    };

    VK_ASSERT(gltfImages.size() == textures.size(), "Texture count mismatch between glTF data and texture array");

    auto enumView = std::views::enumerate(gltfImages);
    std::for_each(std::execution::par, enumView.begin(), enumView.end(), [&](const std::tuple<size_t, const fastgltf::Image&>& pair) {
      const auto& [idx, img] = pair;

      auto fromKtx = [&](ktxTexture2* tex, ktx_error_code_e err) {
        VK_ASSERT(err == KTX_SUCCESS, "Failed to create ktxTexture from memory source: {}", ktxErrorString(err));
        bool needsTranscode = ktxTexture2_NeedsTranscoding(tex);

        if (needsTranscode) {
          auto e = ktxTexture2_TranscodeBasis(tex, KTX_TTF_BC7_RGBA, 0);
          VK_ASSERT(e == KTX_SUCCESS, "Failed to transcode ktxTexture: {}", ktxErrorString(err));
        }

        VK_ASSERT(tex->vkFormat != VK_FORMAT_UNDEFINED, "ktxTexture has undefined Vulkan format");
        VK_ASSERT(tex->pData != nullptr, "ktxTexture data pointer is null");
        VK_ASSERT(tex->dataSize > 0, "ktx image size is 0.");

        std::vector<size_t> dataOffsets(tex->numLevels - 1);
        for (uint32_t level = 1; level < tex->numLevels; ++level) {
          ktxTexture2_GetImageOffset(tex, level, 0, 0, &dataOffsets[level - 1]);
        }

        return Tex{
            .name = std::string(img.name),
            .ktx = tex,
        };
      };

      auto fromMemory = [&](const std::span<const std::byte>& bytes) -> Tex {
        if (isKtxFile(bytes)) {
          ktxTexture2* ktxTexture = nullptr;
          ktx_error_code_e err =
              ktxTexture2_CreateFromMemory(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), createFlags, &ktxTexture);
          return fromKtx(ktxTexture, err);

        } else {
          VK_CRITICAL("Unsupported image source format for image {}, expected ktx2 format in memory", img.name);
          abort();
        }
      };

      textures[idx] = std::visit(
          fastgltf::visitor{
              [](auto& arg) -> Tex {
                VK_CRITICAL("Unsupported glTF image source type {}", typeid(arg).name());
                abort();
              },
              [&](const fastgltf::sources::Array& array) { return fromMemory(array.bytes); },
              [&](const fastgltf::sources::URI& filePath) -> Tex {
                assert(filePath.fileByteOffset == 0);
                assert(filePath.uri.isLocalPath());

                auto extension = filePath.uri.fspath().extension();

                std::filesystem::path assetPath = gltf.basePath / filePath.uri.fspath();

                VK_ASSERT(std::filesystem::exists(assetPath), "Image file does not exist at path: {}", assetPath.string());

                if (extension == ".ktx") {
                  VK_CRITICAL("ktx1 files are not supported, please convert {} to ktx2 format", assetPath.string());
                  abort();
                } else if (extension == ".ktx2") {
                  ktxTexture2* ktxTexture = nullptr;
                  ktx_error_code_e err = ktxTexture2_CreateFromNamedFile(assetPath.string().c_str(), createFlags, &ktxTexture);
                  return fromKtx(ktxTexture, err);
                } else {
                  VK_CRITICAL("Unsupported image file extension {} for image {}, expected .ktx2", extension.string(), img.name);
                  abort();
                }
              },
              [&](const fastgltf::sources::Vector& vector) { return fromMemory(vector.bytes); },
              [&](const fastgltf::sources::BufferView view) -> Tex {
                auto& bufferView = gltf.bufferViews[view.bufferViewIndex];
                auto& buffer = gltf.buffers[bufferView.bufferIndex];

                return std::visit(
                    fastgltf::visitor{
                        // We only care about VectorWithMime here, because
                        // we
                        // specify LoadExternalBuffers, meaning all buffers
                        // are already loaded into a vector.
                        [](auto& arg) -> Tex {
                          VK_CRITICAL("Unsupported glTF image source buffer "
                                      "type {}",
                                      typeid(arg).name());
                          abort();
                        },
                        [&](const fastgltf::sources::Array& array) {
                          return fromMemory(std::span<const std::byte>(array.bytes.data() + bufferView.byteOffset, bufferView.byteLength));
                        },
                        [&](const fastgltf::sources::Vector& vector) {
                          return fromMemory(std::span<const std::byte>(vector.bytes.data() + bufferView.byteOffset, bufferView.byteLength));
                        },
                    },
                    buffer.data);
              },
          },
          img.data);
    });

    struct SBuf {
      size_t size = 0;
      Buffer buffer;
    };
    std::vector<SBuf> stagingBuffers(1);

    for (auto& tex : textures) {
      if (stagingBuffers.back().size + tex.ktx->dataSize > limits::maxMemoryAllocationSize) {
        stagingBuffers.push_back({});
      }
      stagingBuffers.back().size += tex.ktx->dataSize;
    }

    {
      for (auto& sbuf : stagingBuffers) {
        auto res = Buffer::create({sbuf.size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                                   VMA_MEMORY_USAGE_AUTO_PREFER_HOST, "Image staging buffer"});
        if (!res.isOk())
          return std::unexpected("Failed to create staging buffer for image upload");
        sbuf.buffer = std::move(res.value());
      }
    }

    VkDeviceSize offset = 0;
    size_t bufIdx = 0;

    std::vector<Image> result;
    result.reserve(textures.size());

    m.loadedTextures.reserve(m.loadedTextures.size() + textures.size());

    for (const auto& tex : textures) {
      if (offset + tex.ktx->dataSize > stagingBuffers[bufIdx].size) {
        ++bufIdx;
        offset = 0;
      }
      auto& buf = stagingBuffers[bufIdx];
      VK_ASSERT(buf.buffer.isMapped(), "Staging buffer for image upload is not mapped");
      VK_ASSERT(tex.ktx->pData != nullptr, "Texture data is null for image {}", tex.name);
      VK_ASSERT(tex.ktx->dataSize > 0, "Texture size is zero for image {}", tex.name);
      VK_ASSERT(tex.ktx->vkFormat != VK_FORMAT_UNDEFINED, "Texture format is undefined for image {}", tex.name);
      VK_ASSERT(tex.ktx->baseWidth > 0 && tex.ktx->baseHeight > 0, "Texture dimensions are invalid for image {}", tex.name);
      VK_ASSERT(tex.ktx->numLevels > 0, "Texture has no mip levels for image {}", tex.name);

      VK_TRACE("Creating image {} with format {} and extent {}x{}", tex.name, tex.ktx->vkFormat, imageCreateInfo.extent.width,
               imageCreateInfo.extent.height);

      memcpy(buf.buffer.mapping() + offset, tex.ktx->pData, tex.ktx->dataSize);

      auto res = Image::create(ImageCreateInfo(VK_IMAGE_TYPE_2D, (VkFormat)tex.ktx->vkFormat,
                                               VkExtent3D{
                                                   .width = tex.ktx->baseWidth,
                                                   .height = tex.ktx->baseHeight,
                                                   .depth = 1,
                                               },
                                               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, tex.ktx->numLevels, 1,
                                               tex.name.c_str()));
      if (!res.isOk())
        return std::unexpected("Failed to create image for texture");

      loadImage(res.value());
      result.emplace_back(std::move(res.value()));

      auto& image = result.back();

      m.loadedTextures.push_back({
          .image = image,
          .view = image,
          .alloc = image.getAllocation(),
      });

      VkImageMemoryBarrier2 imgTransferBarrier{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
          .srcAccessMask = 0,
          .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
          .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .image = image,
          .subresourceRange =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .baseMipLevel = 0,
                  .levelCount = tex.ktx->numLevels,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
      };

      VkDependencyInfo transferDepInfo{
          .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .imageMemoryBarrierCount = 1,
          .pImageMemoryBarriers = &imgTransferBarrier,
      };
      vkCmdPipelineBarrier2(transferCmd, &transferDepInfo);

      for (uint32_t level = 0; level < tex.ktx->numLevels; ++level) {
        ktx_size_t o = 0;
        ktxTexture2_GetImageOffset(tex.ktx, level, 0, 0, &o);

        VkBufferImageCopy region{
            .bufferOffset = offset + o,
            .imageSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = level,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .imageExtent = {.width = tex.ktx->baseWidth >> level, .height = tex.ktx->baseHeight >> level, .depth = 1},
        };

        vkCmdCopyBufferToImage(transferCmd, buf.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
      }

      VkImageMemoryBarrier2 imgUseBarrier{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
          .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
          .dstAccessMask = VK_ACCESS_2_NONE,
          .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          .image = image,
          .subresourceRange =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .baseMipLevel = 0,
                  .levelCount = tex.ktx->numLevels,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
      };
      VkDependencyInfo useDepInfo{
          .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .imageMemoryBarrierCount = 1,
          .pImageMemoryBarriers = &imgUseBarrier,
      };
      vkCmdPipelineBarrier2(transferCmd, &useDepInfo);

      offset += tex.ktx->dataSize;

      ktxTexture2_Destroy(tex.ktx);

      VK_TRACE("Uploaded image {} to GPU", tex.name);
    }

    std::vector<Buffer> stagingBufs;
    stagingBufs.reserve(stagingBuffers.size());
    for (auto& sbuf : stagingBuffers) {
      stagingBufs.push_back(std::move(sbuf.buffer));
    }
    UploadResult<Image> results{
        .resources = std::move(result),
        .stagingBuffers = std::move(stagingBufs),
    };

    VK_DEBUG("Finished uploading images, total {} bytes in {} staging buffers. Last texture index: {}", offset, stagingBuffers.size(),
             m.indices.nextCombinedImageIndex - 1);

    return std::move(results);
  }

  std::expected<Renderer::UploadResult<Mesh>, std::string>
  Renderer::uploadMeshes(const std::vector<gltf::MeshData>& meshes, const std::vector<Material>& materials,
                         const VkCommandBuffer) { // TODO: Support none ReBAR systems
    KT_PROFILE_FUNCTION
    VK_DEBUG("Uploading {} meshes from glTF data", meshes.size());

    VKH_MAKE(bufs,
             loading::ensureBuffersAreLargeEnough(meshes, m.buffers.vertexPositions, m.buffers.vertexAttribs, m.buffers.indices,
                                                  m.buffers.meshlets, m.buffers.meshletVertices, m.buffers.meshletTriangles),
             "Failed to ensure buffers are large enough for mesh upload");

    bool canWriteDirectly = m.buffers.vertexPositions->isMapped() && m.buffers.vertexAttribs->isMapped() &&
                            m.buffers.meshlets->isMapped() && m.buffers.meshletVertices->isMapped() &&
                            m.buffers.meshletTriangles->isMapped();
    if (!canWriteDirectly) {
      VK_CRITICAL("None ReBAR is not supported yet, please enable ReBAR in your GPU settings and ensure your GPU supports it.");
      abort();
    }

    std::vector<Mesh> result;
    result.reserve(meshes.size());
    std::vector<Submesh> submeshes;

    for (const auto& mesh : meshes) {
      submeshes.clear();
      submeshes.reserve(mesh.submeshes.size());

      VKH_MAKE(vertexOffsets, loading::uploadVertices(mesh, m.buffers.vertexPositions, m.buffers.vertexAttribs),
               "Failed to upload vertices for mesh");
      VKH_MAKE(indexOffset, loading::uploadIndices(mesh, m.buffers.indices), "Failed to upload indices for mesh");
      VKH_MAKE(meshletOffsets, loading::uploadMeshlets(mesh, m.buffers.meshlets, m.buffers.meshletVertices, m.buffers.meshletTriangles),
               "Failed to upload meshlets for mesh");

      bufs.reserve(bufs.size() + vertexOffsets.reallocatedBuffers.size() + indexOffset.reallocatedBuffers.size() +
                   meshletOffsets.reallocatedBuffers.size());
      for (auto& buf : vertexOffsets.reallocatedBuffers) {
        bufs.push_back(std::move(buf));
      }
      for (auto& buf : indexOffset.reallocatedBuffers) {
        bufs.push_back(std::move(buf));
      }
      for (auto& buf : meshletOffsets.reallocatedBuffers) {
        bufs.push_back(std::move(buf));
      }

      for (const auto& primitive : mesh.submeshes) {
        Submesh submesh{
            .vertexOffset = static_cast<int32_t>(primitive.vertex.offset + vertexOffsets.result),
            .meshletOffset = static_cast<uint32_t>(primitive.meshlet.offset + meshletOffsets.result.meshlet),
            .meshletCount = primitive.meshlet.count,
            .meshletVertexOffset = static_cast<uint32_t>(primitive.meshlet.vertexOffset + meshletOffsets.result.vertex),
            .meshletTriangleOffset = static_cast<uint32_t>(primitive.meshlet.triangleOffset + meshletOffsets.result.triangle),
            .vertexCount = primitive.vertex.count,
            .meshletVertexCount = primitive.meshlet.vertexCount,
            .meshletTriangleCount = primitive.meshlet.triangleCount,
            .boundingSphere = primitive.boundingSphere,
            .id = m.nextMeshIndex++,
        };
        if (primitive.materialIndex < materials.size()) {
          submesh.material = materials[primitive.materialIndex];
        }
        submeshes.push_back(submesh);
      }

      Mesh resultMesh(static_cast<uint32_t>(mesh.positions.size()), submeshes, mesh.name);
      result.push_back(std::move(resultMesh));
    }

    UploadResult<Mesh> resultStruct{
        .resources = std::move(result),
        .stagingBuffers = std::move(bufs),
    };

    return std::move(resultStruct);
  }

  std::expected<Renderer::UploadResult<Material>, std::string>
  Renderer::createMaterials(const gltf::Data& data, const std::vector<Image>& textures, const VkCommandBuffer transferCmd) {
    KT_PROFILE_FUNCTION
    auto& materials = data.materials;

    auto getTexture = [&](const fastgltf::TextureInfo& i) -> const Image* {
      auto& tex = data.textures[i.textureIndex];
      if (tex.basisuImageIndex.has_value()) {
        auto idx = tex.basisuImageIndex.value();
        return &textures[idx];
      } else {
        VK_ASSERT(tex.imageIndex.has_value(), "Texture {} has no image index", tex.name);
        return &textures[tex.imageIndex.value()];
      }
    };

    auto toGlmVec4 = [](const fastgltf::math::nvec4& g) { return glm::vec4(g.x(), g.y(), g.z(), g.w()); };
    auto toGlmVec3 = [](const fastgltf::math::nvec3& g) { return glm::vec3(g.x(), g.y(), g.z()); };

    std::vector<Material> result;
    result.reserve(materials.size());
    std::vector<GpuMaterial> gpuMaterials;
    gpuMaterials.reserve(materials.size());

    uint32_t materialIndex = static_cast<uint32_t>(m.buffers.materials.count());

    for (const auto& mat : materials) {
      VK_TRACE("Creating material {}", mat.name);

      MaterialLayer matLayer{
          .albedoFactor = toGlmVec4(mat.pbrData.baseColorFactor),
          .emissiveFactor = toGlmVec3(mat.emissiveFactor),
          .metFactor = mat.pbrData.metallicFactor,
          .roughFactor = mat.pbrData.roughnessFactor,
          .alphaCutoff = mat.alphaCutoff,
      };

      if (mat.pbrData.baseColorTexture.has_value()) {
        matLayer.albedo = getTexture(mat.pbrData.baseColorTexture.value());
      }
      if (mat.normalTexture.has_value()) {
        matLayer.bump = getTexture(mat.normalTexture.value());
      }
      if (mat.pbrData.metallicRoughnessTexture.has_value()) {
        matLayer.metRough = getTexture(mat.pbrData.metallicRoughnessTexture.value());
      }
      if (mat.emissiveTexture.has_value()) {
        matLayer.emissive = getTexture(mat.emissiveTexture.value());
      }
      if (mat.occlusionTexture.has_value()) {
        matLayer.ao = getTexture(mat.occlusionTexture.value());
      }
      if (mat.specular != nullptr) {
        matLayer.specFactor = mat.specular->specularFactor;
      }

      GpuMaterial gpuMat{
          .albedo = matLayer.albedo->handle(),
          .bump = matLayer.bump->handle(),
          .emissive = matLayer.emissive->handle(),
          .metRough = matLayer.metRough->handle(),
          .albedoFactor = matLayer.albedoFactor,
          .emissiveFactor = matLayer.emissiveFactor,
          .ao = matLayer.ao->handle(),
          .metFactor = matLayer.metFactor,
          .roughFactor = matLayer.roughFactor,
          .specFactor = matLayer.specFactor,
          .alphaCutoff = matLayer.alphaCutoff,
      };

      gpuMaterials.push_back(gpuMat);

      result.emplace_back(materialIndex++);
    }

    UploadResult<Material> resultStruct{
        .resources = std::move(result),
    };

    if (m.buffers.materials->isMapped()) {
      m.buffers.materials.write(gpuMaterials);
    } else {
      const size_t stagingBufferSize = gpuMaterials.size() * sizeof(GpuMaterial);
      auto res = Buffer::create({stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                                 VMA_MEMORY_USAGE_AUTO_PREFER_HOST, "Material staging buffer"});
      if (!res.isOk())
        return std::unexpected("Failed to create staging buffer for material upload");

      resultStruct.stagingBuffers.push_back(std::move(res.value()));
      auto& stagingBuffer = resultStruct.stagingBuffers.back();

      memcpy(stagingBuffer.mapping(), gpuMaterials.data(), stagingBufferSize);
      VkBufferCopy copy{
          .srcOffset = 0,
          .dstOffset = m.buffers.materials.count() * sizeof(GpuMaterial),
          .size = stagingBufferSize,
      };
      vkCmdCopyBuffer(transferCmd, stagingBuffer, m.buffers.materials, 1, &copy);
    }

    return std::move(resultStruct);
  }
} // namespace kt::rdr
