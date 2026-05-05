#include "keptech/vulkan/renderer.hpp"
#include "stb/image.h"

#include "keptech/vulkan/constants.hpp"
#include "keptech/vulkan/helpers/formatting.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include "profile.hpp"
#include "vk-logger.hpp"
#include <cstring>
#include <execution>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/components/camera.hpp>
#include <keptech/core/window.hpp>
#include <keptech/rendering/gltf/data.hpp>
#include <keptech/rendering/gltf/scene.hpp>
#include <ktx.h>
#include <ktxvulkan.h>

namespace kt::vkh {
  bool Renderer::canRenderToFormat(VkFormat format) const {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(m.vkcore.device.physical, format, &formatProps);
    return ((formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT) |
            (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT)) != 0;
  }

  namespace {
    gltf::Scene::Node createNode(const gltf::Data::Node& node, const std::vector<Mesh>& meshes) {
      gltf::Scene::Node result{
          .name = std::string(node.node.name),
          .transform = node.transform,
          .mesh = node.meshIndex == ~0 ? Mesh() : meshes[node.meshIndex],
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

    VkCommandBuffer transferCmd = nullptr;
    VkCommandBufferAllocateInfo cmdInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m.vkcore.transferPool.pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VK_CHECK(vkAllocateCommandBuffers(m.vkcore.device.logical, &cmdInfo, &transferCmd),
             "Failed to allocate command buffer for mesh upload");

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
    VK_CHECK(vkCreateFence(m.vkcore.device.logical, &fenceCreateInfo, nullptr, &transferFence), "Failed to create fence for mesh upload");
    vkQueueSubmit2(m.vkcore.queues.transfer.queue, 1, &submitInfo, transferFence);

    VkResult res = VK_SUCCESS;
    {
      KT_PROFILE_SCOPE("Wait for Mesh Upload");
      while (res = vkWaitForFences(m.vkcore.device.logical, 1, &transferFence, VK_TRUE, UINT64_MAX), res == VK_TIMEOUT) {
        std::this_thread::yield();
      }
    }
    VK_ASSERT(res == VK_SUCCESS, "Failed to wait for mesh upload fence");
    vkDestroyFence(m.vkcore.device.logical, transferFence, nullptr);
    vkFreeCommandBuffers(m.vkcore.device.logical, m.vkcore.transferPool.pool, 1, &transferCmd);
    for (auto& buf : meshesRes.stagingBuffers) {
      buf.destroy(m.vkcore.allocator);
    }
    for (auto& buf : imagesRes.stagingBuffers) {
      buf.destroy(m.vkcore.allocator);
    }
    for (auto& buf : materialsRes.stagingBuffers) {
      buf.destroy(m.vkcore.allocator);
    }

    gltf::Scene scene{};
    scene.roots.reserve(gltfData.roots.size());

    for (const auto& node : gltfData.roots) {
      scene.roots.push_back(createNode(node, meshesRes.resources));
    }

    return scene;
  }

  std::expected<Renderer::UploadResult<Texture>, std::string> Renderer::createImages(const gltf::Data& gltf,
                                                                                     const VkCommandBuffer transferCmd) {
    KT_PROFILE_FUNCTION
    auto& gltfImages = gltf.images;
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
          auto err = ktxTexture2_TranscodeBasis(tex, KTX_TTF_BC7_RGBA, 0);
          VK_ASSERT(err == KTX_SUCCESS, "Failed to transcode ktxTexture: {}", ktxErrorString(err));
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
      AllocatedBuffer buffer;
    };
    std::vector<SBuf> stagingBuffers(1);

    for (auto& tex : textures) {
      if (stagingBuffers.back().size + tex.ktx->dataSize > limits::maxMemoryAllocationSize) {
        stagingBuffers.push_back({});
      }
      stagingBuffers.back().size += tex.ktx->dataSize;
    }

    {
      VkBufferCreateInfo stagingBufferCreateInfo{
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      };

      VmaAllocationCreateInfo stagingAllocInfo{
          .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
          .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
      };

      for (auto& sbuf : stagingBuffers) {
        stagingBufferCreateInfo.size = sbuf.size;
        VKH_MAKE(buffer,
                 AllocatedBuffer::create(m.vkcore.allocator, m.vkcore.device.logical, stagingBufferCreateInfo, stagingAllocInfo,
                                         "Image staging buffer."),
                 "Failed to create staging buffer for image upload");
        sbuf.buffer = buffer;
      }
    }

    VkDeviceSize offset = 0;
    size_t bufIdx = 0;
    VkImageCreateInfo imageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImageViewCreateInfo imageViewCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    VmaAllocationCreateInfo imageAllocInfo{
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    std::vector<Texture> result;
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

      imageCreateInfo.format = (VkFormat)tex.ktx->vkFormat;
      imageCreateInfo.extent = VkExtent3D{
          .width = tex.ktx->baseWidth,
          .height = tex.ktx->baseHeight,
          .depth = 1,
      };
      imageCreateInfo.mipLevels = tex.ktx->numLevels;
      imageViewCreateInfo.subresourceRange.levelCount = imageCreateInfo.mipLevels;

      VK_TRACE("Creating image {} with format {} and extent {}x{}", tex.name, tex.format, imageCreateInfo.extent.width,
               imageCreateInfo.extent.height);

      memcpy(buf.buffer.mapping() + offset, tex.ktx->pData, tex.ktx->dataSize);

      VKH_MAKE(image,
               AllocatedImage::create(m.vkcore.allocator, m.vkcore.device.logical, imageCreateInfo, imageAllocInfo, imageViewCreateInfo,
                                      true, tex.name),
               "Failed to create image for texture");

      auto index = m.nextTextureIndex++;
      result.emplace_back(Texture::Type::e2D, glm::ivec3{tex.ktx->baseWidth, tex.ktx->baseHeight, 1}, 1, (VkFormat)tex.ktx->vkFormat, index,
                          image);

      m.loadedTextures.push_back(image);

      for (auto& frame : m.vkcore.perFrame) {
        frame.texToUpdate.emplace_back(image, index);
      }

      VkImageMemoryBarrier2 imgTransferBarrier{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
          .srcAccessMask = 0,
          .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
          .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .image = image.image,
          .subresourceRange =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .baseMipLevel = 0,
                  .levelCount = imageCreateInfo.mipLevels,
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

        vkCmdCopyBufferToImage(transferCmd, buf.buffer.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
      }

      VkImageMemoryBarrier2 imgUseBarrier{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
          .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
          .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
          .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          .image = image.image,
          .subresourceRange =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .baseMipLevel = 0,
                  .levelCount = imageCreateInfo.mipLevels,
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

    std::vector<AllocatedBuffer> stagingBufs;
    stagingBufs.reserve(stagingBuffers.size());
    for (auto& sbuf : stagingBuffers) {
      stagingBufs.push_back(sbuf.buffer);
    }
    UploadResult<Texture> results{
        .resources = std::move(result),
        .stagingBuffers = std::move(stagingBufs),
    };

    VK_DEBUG("Finished uploading images, total {} bytes in {} staging buffers. Last texture index: {}", offset, stagingBuffers.size(),
             m.nextTextureIndex - 1);

    return std::move(results);
  }

  std::expected<Renderer::UploadResult<Mesh>, std::string> Renderer::uploadMeshes(const std::vector<gltf::MeshData>& meshes,
                                                                                  const std::vector<rendering::Material>& materials,
                                                                                  const VkCommandBuffer transferCmd) {
    KT_PROFILE_FUNCTION
    VK_DEBUG("Uploading {} meshes from glTF data", meshes.size());

    size_t newVertexCount = 0;
    size_t newIndexCount = 0;

    for (const auto& mesh : meshes) {
      newVertexCount += mesh.vertices.size();
      newIndexCount += mesh.indices.size();
    }

    size_t newVerticesSize = newVertexCount * sizeof(Vertex);
    size_t newIndicesSize = newIndexCount * sizeof(uint32_t);

    size_t totalVerticesCount = m.buffers.vertices.count + newVertexCount;
    size_t totalIndicesCount = m.buffers.indices.count + newIndexCount;

    size_t totalVerticesSize = totalVerticesCount * sizeof(Vertex);
    size_t totalIndicesSize = totalIndicesCount * sizeof(uint32_t);

    std::optional<SubdivBuffer<Vertex>> oldVertexBuffer;
    std::optional<SubdivBuffer<uint32_t>> oldIndexBuffer;
    if (totalVerticesSize > m.buffers.vertices.buffer.size()) {
      VK_DEBUG("Current vertex buffer size {} is too small for {} vertices, creating new buffer", m.buffers.vertices.buffer.size(),
               totalVerticesCount);
      oldVertexBuffer = m.buffers.vertices;
      VkBufferCreateInfo vertexBufferCreateInfo{
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .size = totalVerticesSize,
          .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      };
      VmaAllocationCreateInfo vertexAllocInfo{
          .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                   VMA_ALLOCATION_CREATE_MAPPED_BIT,
          .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      };
      VKH_MAKE(vertexBuffer,
               AddressedAllocatedBuffer::create(m.vkcore.device.logical, m.vkcore.allocator, vertexBufferCreateInfo, vertexAllocInfo,
                                                "Mesh vertex buffer"),
               "Failed to create vertex buffer for mesh upload");
      m.buffers.vertices = SubdivBuffer<Vertex>{
          .buffer = vertexBuffer,
          .count = oldVertexBuffer->count,
      };
    }
    if (totalIndicesSize > m.buffers.indices.buffer.size()) {
      VK_DEBUG("Current index buffer size {} is too small for {} indices, creating new buffer", m.buffers.indices.buffer.size(),
               totalIndicesCount);
      oldIndexBuffer = m.buffers.indices;
      VkBufferCreateInfo indexBufferCreateInfo{
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .size = totalIndicesSize,
          .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      };
      VmaAllocationCreateInfo allocInfo{
          .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                   VMA_ALLOCATION_CREATE_MAPPED_BIT,
          .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      };
      VKH_MAKE(indexBuffer,
               AddressedAllocatedBuffer::create(m.vkcore.device.logical, m.vkcore.allocator, indexBufferCreateInfo, allocInfo,
                                                "Mesh index buffer"),
               "Failed to create index buffer for mesh upload");
      m.buffers.indices = SubdivBuffer<uint32_t>{
          .buffer = indexBuffer,
          .count = oldIndexBuffer->count,
      };
    }

    bool canWriteDirectly = m.buffers.vertices.buffer.isMapped() && m.buffers.indices.buffer.isMapped();

    std::vector<Mesh> result;
    result.reserve(meshes.size());

    struct RequiredCopy {
      size_t mesh;
      size_t vertexOffset;
      size_t indexOffset;
    };
    size_t requiredStagingSize = 0;
    std::vector<RequiredCopy> requiredCopies{};

    auto addSubmeshes = [&](const gltf::MeshData& mesh) {
      std::vector<Submesh> submeshes;
      submeshes.reserve(mesh.submeshes.size());
      for (const auto& primitive : mesh.submeshes) {
        Submesh submesh{
            .start = primitive.indexOffset,
            .count = primitive.indexCount,
            .boundingSphere = primitive.boundingSphere,
        };
        if (primitive.materialIndex < materials.size()) {
          submesh.material = materials[primitive.materialIndex];
        }
        submeshes.push_back(submesh);
      }
      return submeshes;
    };

    RendererMesh rendererMesh{
        .firstVertex = m.buffers.vertices.count,
        .firstIndex = m.buffers.indices.count,
    };

    if (canWriteDirectly) {
      VK_DEBUG("Vertex and index buffers are mapped, writing mesh data directly to buffers");
      for (const auto& mesh : meshes) {

        m.buffers.vertices.write(mesh.vertices);
        m.buffers.indices.write(mesh.indices);

        auto submeshes = addSubmeshes(mesh);

        result.emplace_back(mesh.vertices.size(), mesh.indices.size(), rendererMesh, std::move(submeshes), mesh.name);

        rendererMesh.firstVertex += mesh.vertices.size();
        rendererMesh.firstIndex += mesh.indices.size();
      }
    } else {
      VK_DEBUG("Vertex and index buffers are not mapped, staging mesh data in CPU memory for upload");
      for (const auto& mesh : meshes) {
        size_t vertexBufferSize = mesh.vertices.size() * sizeof(Vertex);
        size_t indexBufferSize = mesh.indices.size() * sizeof(uint32_t);

        requiredCopies.push_back(RequiredCopy{
            .mesh = result.size(),
            .vertexOffset = requiredStagingSize,
            .indexOffset = requiredStagingSize + vertexBufferSize,
        });
        requiredStagingSize += vertexBufferSize + indexBufferSize;
        auto submeshes = addSubmeshes(mesh);

        result.emplace_back(mesh.vertices.size(), mesh.indices.size(), rendererMesh, std::move(submeshes), mesh.name);

        rendererMesh.firstVertex += mesh.vertices.size();
        rendererMesh.firstIndex += mesh.indices.size();
      }
    }

    UploadResult<Mesh> resultStruct{};
    if (oldVertexBuffer.has_value()) {
      VK_DEBUG("Old vertex buffer {} | New vertex buffer {}", oldVertexBuffer->buffer.isMapped() ? "mapped" : "not mapped",
               m.buffers.vertices.buffer.isMapped() ? "mapped" : "not mapped");
      if (oldVertexBuffer->buffer.isMapped() && m.buffers.vertices.buffer.isMapped()) {
        uint8_t* srcPtr = oldVertexBuffer->buffer.mapping();
        uint8_t* dstPtr = m.buffers.vertices.buffer.mapping();
        memcpy(dstPtr, srcPtr, oldVertexBuffer->count * sizeof(Vertex));
      } else {
        VkBufferCopy copyRegion{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = oldVertexBuffer->count * sizeof(Vertex),
        };
        vkCmdCopyBuffer(transferCmd, oldVertexBuffer->buffer.buffer, m.buffers.vertices.buffer.buffer, 1, &copyRegion);
      }
      resultStruct.stagingBuffers.push_back(oldVertexBuffer->buffer.downcast());
    }
    if (oldIndexBuffer.has_value()) {
      VK_DEBUG("Old index buffer {} | New index buffer {}", oldIndexBuffer->buffer.isMapped() ? "mapped" : "not mapped",
               m.buffers.indices.buffer.isMapped() ? "mapped" : "not mapped");
      if (oldIndexBuffer->buffer.isMapped() && m.buffers.indices.buffer.isMapped()) {
        uint8_t* srcPtr = oldIndexBuffer->buffer.mapping();
        uint8_t* dstPtr = m.buffers.indices.buffer.mapping();
        memcpy(dstPtr, srcPtr, oldIndexBuffer->count * sizeof(uint32_t));
      } else {
        VkBufferCopy copyRegion{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = oldIndexBuffer->count * sizeof(uint32_t),
        };
        vkCmdCopyBuffer(transferCmd, oldIndexBuffer->buffer.buffer, m.buffers.indices.buffer.buffer, 1, &copyRegion);
      }
      resultStruct.stagingBuffers.push_back(oldIndexBuffer->buffer.downcast());
    }

    if (requiredStagingSize > 0) {
      VkBufferCreateInfo stagingBufferCreateInfo{
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .size = requiredStagingSize,
          .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      };

      VmaAllocationCreateInfo stagingAllocInfo{
          .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
          .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
      };

      VKH_MAKE(stagingBuffer,
               AllocatedBuffer::create(m.vkcore.allocator, m.vkcore.device.logical, stagingBufferCreateInfo, stagingAllocInfo,
                                       "Mesh staging buffer."),
               "Failed to create staging buffer for mesh upload");

      if (stagingBuffer.isMapped()) {
        uint8_t* mappedPtr = static_cast<uint8_t*>(stagingBuffer.mapping());
        for (const auto& copy : requiredCopies) {
          const auto& mesh = meshes[copy.mesh];
          memcpy(mappedPtr + copy.vertexOffset, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
          memcpy(mappedPtr + copy.indexOffset, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));
        }
      } else {
        stagingBuffer.destroy(m.vkcore.allocator);
        return std::unexpected("Failed to map staging buffer for mesh upload");
      }

      VkBufferCopy copyRegion{};
      for (const auto& copy : requiredCopies) {
        const auto& mesh = result[copy.mesh];
        copyRegion.srcOffset = copy.vertexOffset;
        copyRegion.dstOffset = mesh.getRMesh().firstVertex * sizeof(Vertex);
        copyRegion.size = mesh.getVertexCount() * sizeof(Vertex);
        vkCmdCopyBuffer(transferCmd, stagingBuffer.buffer, m.buffers.vertices.buffer.buffer, 1, &copyRegion);
        m.buffers.vertices.count += mesh.getVertexCount();
        copyRegion.srcOffset = copy.indexOffset;
        copyRegion.dstOffset = mesh.getRMesh().firstIndex * sizeof(uint32_t);
        copyRegion.size = mesh.getIndexCount() * sizeof(uint32_t);
        vkCmdCopyBuffer(transferCmd, stagingBuffer.buffer, m.buffers.indices.buffer.buffer, 1, &copyRegion);
        m.buffers.indices.count += mesh.getIndexCount();
      }

      resultStruct.stagingBuffers.push_back(stagingBuffer);
    }

    resultStruct.resources = std::move(result);

    return std::move(resultStruct);
  }

  std::expected<Renderer::UploadResult<rendering::Material>, std::string>
  Renderer::createMaterials(const gltf::Data& data, const std::vector<Texture>& textures, const VkCommandBuffer transferCmd) {
    KT_PROFILE_FUNCTION
    auto& materials = data.materials;

    auto getTexture = [&](const fastgltf::TextureInfo& i) -> const Texture& {
      auto& tex = data.textures[i.textureIndex];
      if (tex.basisuImageIndex.has_value()) {
        auto idx = tex.basisuImageIndex.value();
        return textures[idx];
      } else {
        VK_ASSERT(tex.imageIndex.has_value(), "Texture {} has no image index", tex.name);
        return textures[tex.imageIndex.value()];
      }
    };

    auto toGlmVec4 = [](const fastgltf::math::nvec4& g) { return glm::vec4(g.x(), g.y(), g.z(), g.w()); };
    auto toGlmVec3 = [](const fastgltf::math::nvec3& g) { return glm::vec3(g.x(), g.y(), g.z()); };

    std::vector<rendering::Material> result;
    result.reserve(materials.size());
    std::vector<GpuMaterial> gpuMaterials;
    gpuMaterials.reserve(materials.size());

    uint32_t materialIndex = m.buffers.materials.count;

    for (const auto& mat : materials) {
      VK_TRACE("Creating material {}", mat.name);

      rendering::MaterialLayer matLayer{
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
          .albedo = matLayer.albedo.getIndex(),
          .bump = matLayer.bump.getIndex(),
          .emissive = matLayer.emissive.getIndex(),
          .metRough = matLayer.metRough.getIndex(),
          .albedoFactor = matLayer.albedoFactor,
          .emissiveFactor = matLayer.emissiveFactor,
          .ao = matLayer.ao.getIndex(),
          .metFactor = matLayer.metFactor,
          .roughFactor = matLayer.roughFactor,
          .specFactor = matLayer.specFactor,
          .alphaCutoff = matLayer.alphaCutoff,
      };

      gpuMaterials.push_back(gpuMat);

      result.emplace_back(materialIndex++);
    }

    UploadResult<rendering::Material> resultStruct{
        .resources = std::move(result),
    };

    if (m.buffers.materials.buffer.isMapped()) {
      m.buffers.materials.write(gpuMaterials);
    } else {
      const size_t stagingBufferSize = gpuMaterials.size() * sizeof(GpuMaterial);
      VkBufferCreateInfo stagingBufferCreateInfo{
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .size = stagingBufferSize,
          .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      };
      VmaAllocationCreateInfo stagingAllocInfo{
          .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
          .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
      };
      VKH_MAKE(stagingBuffer,
               AllocatedBuffer::create(m.vkcore.allocator, m.vkcore.device.logical, stagingBufferCreateInfo, stagingAllocInfo,
                                       "Material staging buffer"),
               "Failed to create staging buffer for material upload");

      resultStruct.stagingBuffers.push_back(stagingBuffer);

      memcpy(stagingBuffer.mapping(), gpuMaterials.data(), stagingBufferSize);
      VkBufferCopy copy{
          .srcOffset = 0,
          .dstOffset = m.buffers.materials.count * sizeof(GpuMaterial),
          .size = stagingBufferSize,
      };
      vkCmdCopyBuffer(transferCmd, stagingBuffer.buffer, m.buffers.materials.buffer.buffer, 1, &copy);
    }

    return std::move(resultStruct);
  }
} // namespace kt::vkh
