#include "keptech/vulkan/renderer.hpp"

#include "keptech/vulkan/constants.hpp"
#include "keptech/vulkan/helpers/formatting.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
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

    VKH_MAKE(meshesRes, uploadMeshes(gltfData.meshes, transferCmd), "Failed to upload meshes from glTF data");
    VKH_MAKE(imagesRes, createImages(gltfData.images, gltfData, transferCmd), "Failed to create images from glTF data");

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

    VkResult res;
    while (res = vkWaitForFences(m.vkcore.device.logical, 1, &transferFence, VK_TRUE, UINT64_MAX), res == VK_TIMEOUT) {
      std::this_thread::yield();
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

    gltf::Scene scene{};
    scene.roots.reserve(gltfData.roots.size());

    for (const auto& node : gltfData.roots) {
      scene.roots.push_back(createNode(node, meshesRes.resources));
    }

    return scene;
  }

  std::expected<Renderer::UploadResult<Texture>, std::string>
  Renderer::createImages(const std::vector<fastgltf::Image>& gltfImages, const gltf::Data& gltf, const VkCommandBuffer transferCmd) {
    VK_DEBUG("Loading {} images from glTF data", gltfImages.size());
    struct Tex {
      std::string name;
      size_t size = 0;
      uint8_t* data = nullptr;
      VkFormat format = VK_FORMAT_UNDEFINED;
      uint32_t width = 0;
      uint32_t height = 0;
      ktxTexture2* ktx = nullptr;
    };

    std::vector<Tex> textures(gltfImages.size());
    constexpr ktxTextureCreateFlags createFlags = KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT;

    auto isKtxFile = [](const std::span<const std::byte>& bytes) {
      return bytes.size() >= 12 && memcmp(bytes.data(), "\xABKTX 11\xBB\r\n\x1A\n", 12) == 0;
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
        auto size = ktxTexture_GetImageSize(reinterpret_cast<ktxTexture*>(tex), 0);
        VK_ASSERT(size > 0, "ktx image size is 0.");

        return Tex{
            .name = std::string(img.name),
            .size = size,
            .data = tex->pData,
            .format = static_cast<VkFormat>(tex->vkFormat),
            .width = tex->baseWidth,
            .height = tex->baseHeight,
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
          int width = 0, height = 0, channels = 0;
          auto data = stbi_load_from_memory(reinterpret_cast<const uint8_t*>(bytes.data()), static_cast<int>(bytes.size()), &width, &height,
                                            &channels, 4);
          if (!data) {
            VK_CRITICAL("Failed to load image from memory: {}", stbi_failure_reason());
            abort();
          }
          return Tex{
              .name = std::string(img.name),
              .size = static_cast<size_t>(width * height * channels),
              .data = data,
              .format = VK_FORMAT_R8G8B8A8_UNORM,
              .width = static_cast<uint32_t>(width),
              .height = static_cast<uint32_t>(height),
          };
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
                  int width = 0, height = 0, channels = 0;
                  uint8_t* data = stbi_load(assetPath.string().c_str(), &width, &height, &channels, 4);
                  if (!data) {
                    VK_CRITICAL("Failed to load image from file {}: {}", assetPath.string(), stbi_failure_reason());
                    abort();
                  }
                  return Tex{
                      .name = std::string(img.name),
                      .size = static_cast<size_t>(width * height * channels),
                      .data = data,
                      .format = VK_FORMAT_R8G8B8A8_UNORM,
                      .width = static_cast<uint32_t>(width),
                      .height = static_cast<uint32_t>(height),
                  };
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
      if (stagingBuffers.back().size + tex.size > limits::maxMemoryAllocationSize) {
        stagingBuffers.push_back({});
      }
      stagingBuffers.back().size += tex.size;
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
        VKH_MAKE(buffer, AllocatedBuffer::create(m.vkcore.allocator, m.vkcore.device.logical, stagingBufferCreateInfo, stagingAllocInfo),
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
      if (offset + tex.size > stagingBuffers[bufIdx].size) {
        ++bufIdx;
        offset = 0;
      }
      auto& buf = stagingBuffers[bufIdx];
      VK_ASSERT(buf.buffer.isMapped(), "Staging buffer for image upload is not mapped");
      VK_ASSERT(tex.data != nullptr, "Texture data is null for image {}", tex.name);
      VK_ASSERT(tex.size > 0, "Texture size is zero for image {}", tex.name);
      VK_ASSERT(tex.format != VK_FORMAT_UNDEFINED, "Texture format is undefined for image {}", tex.name);
      VK_ASSERT(offset + tex.size <= buf.size, "Texture data does not fit in staging buffer for image {}", tex.name);

      imageCreateInfo.format = tex.format;
      imageCreateInfo.extent = VkExtent3D{
          .width = tex.width,
          .height = tex.height,
          .depth = 1,
      };

      VK_DEBUG("Creating image {} with format {} and extent {}x{}", tex.name, tex.format, imageCreateInfo.extent.width,
               imageCreateInfo.extent.height);

      memcpy(buf.buffer.mapping() + offset, tex.data, tex.size);

      VKH_MAKE(image,
               AllocatedImage::create(m.vkcore.allocator, m.vkcore.device.logical, imageCreateInfo, imageAllocInfo, imageViewCreateInfo,
                                      true, tex.name),
               "Failed to create image for texture");

      auto index = m.nextTextureIndex++;
      result.emplace_back(glm::ivec3{tex.width, tex.height, 1}, 1, tex.format, index);

      m.loadedTextures.push_back(image);

      for (auto& frame : m.vkcore.perFrame) {
        frame.texToUpdate.emplace_back(image, index);
      }

      VkImageMemoryBarrier2 imgTransferBarrier{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
          .srcAccessMask = 0,
          .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
          .dstAccessMask = 0,
          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .image = image.image,
          .subresourceRange =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .baseMipLevel = 0,
                  .levelCount = 1,
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

      VkBufferImageCopy copyRegion{
          .bufferOffset = offset,
          .bufferRowLength = 0,
          .bufferImageHeight = 0,
          .imageSubresource =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .mipLevel = 0,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
          .imageOffset = {.x = 0, .y = 0, .z = 0},
          .imageExtent = imageCreateInfo.extent,
      };

      vkCmdCopyBufferToImage(transferCmd, buf.buffer.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

      VkImageMemoryBarrier2 imgUseBarrier{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
          .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
          .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT,
          .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          .image = image.image,
          .subresourceRange =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .baseMipLevel = 0,
                  .levelCount = 1,
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

      offset += tex.size;

      if (tex.ktx)
        ktxTexture2_Destroy(tex.ktx);
      else
        stbi_image_free(tex.data);

      VK_DEBUG("Uploaded image {} to GPU", tex.name);
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

    return std::move(results);
  }

  std::expected<std::vector<Texture>, std::string> Renderer::createImages(const std::vector<ImageUploadInfo>& infos) {
    // TODO: Impl
    return {};
  }

  std::expected<Renderer::UploadResult<Mesh>, std::string> Renderer::uploadMeshes(const std::vector<gltf::MeshData>& meshes,
                                                                                  const VkCommandBuffer transferCmd) {
    VK_DEBUG("Uploading {} meshes from glTF data", meshes.size());
    std::vector<Mesh> result;
    result.reserve(meshes.size());

    struct RequiredCopy {
      size_t mesh;
      size_t vertexOffset;
      size_t indexOffset;
    };
    size_t requiredStagingSize = 0;
    std::vector<RequiredCopy> requiredCopies{};

    for (const auto& mesh : meshes) {
      size_t vertexBufferSize = mesh.vertices.size() * sizeof(Vertex);
      size_t indexBufferSize = mesh.indices.size() * sizeof(uint32_t);

      VkBufferCreateInfo vertexBufferCreateInfo{
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .size = vertexBufferSize,
          .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      };
      VkBufferCreateInfo indexBufferCreateInfo{
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .size = indexBufferSize,
          .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      };

      VmaAllocationCreateInfo allocInfo{
          .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                   VMA_ALLOCATION_CREATE_MAPPED_BIT,
          .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      };

      VKH_MAKE(vertexBuffer,
               AddressedAllocatedBuffer::create(m.vkcore.device.logical, m.vkcore.allocator, vertexBufferCreateInfo, allocInfo,
                                                mesh.name + "_vertex"),
               "Failed to create vertex buffer for mesh");
      m.loadedBuffers.push_back(vertexBuffer.downcast());
      VKH_MAKE(indexBuffer,
               AddressedAllocatedBuffer::create(m.vkcore.device.logical, m.vkcore.allocator, indexBufferCreateInfo, allocInfo,
                                                mesh.name + "_index"),
               "Failed to create index buffer for mesh");
      m.loadedBuffers.push_back(indexBuffer.downcast());

      if (vertexBuffer.isMapped() && indexBuffer.isMapped()) {
        memcpy(vertexBuffer.mapping(), mesh.vertices.data(), vertexBufferSize);
        memcpy(indexBuffer.mapping(), mesh.indices.data(), indexBufferSize);
      } else {
        requiredCopies.push_back(
            {.mesh = result.size(), .vertexOffset = requiredStagingSize, .indexOffset = requiredStagingSize + vertexBufferSize});
        requiredStagingSize += vertexBufferSize + indexBufferSize;
      }

      RendererMesh rendererMesh{
          .vertexBuffer = vertexBuffer,
          .indexBuffer = indexBuffer,
      };

      result.emplace_back(mesh.vertices.size(), 0, mesh.indices.size(), 0, rendererMesh, mesh.name);
    }

    UploadResult<Mesh> resultStruct{};
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
               AllocatedBuffer::create(m.vkcore.allocator, m.vkcore.device.logical, stagingBufferCreateInfo, stagingAllocInfo),
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

      VkCommandBuffer transferCmd = nullptr;
      VkCommandBufferAllocateInfo cmdAllocInfo{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          .commandPool = m.vkcore.transferPool.pool,
          .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          .commandBufferCount = 1,
      };
      VK_CHECK(vkAllocateCommandBuffers(m.vkcore.device.logical, &cmdAllocInfo, &transferCmd),
               "Failed to allocate command buffer for mesh upload");
      VkCommandBufferBeginInfo cmdBeginInfo{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
          .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      };
      vkBeginCommandBuffer(transferCmd, &cmdBeginInfo);

      VkBufferCopy copyRegion{
          .dstOffset = 0,
      };
      for (const auto& copy : requiredCopies) {
        const auto& mesh = result[copy.mesh];
        copyRegion.srcOffset = copy.vertexOffset;
        copyRegion.size = mesh.getVertexCount() * sizeof(Vertex);
        vkCmdCopyBuffer(transferCmd, stagingBuffer.buffer, mesh.getRMesh().vertexBuffer.buffer, 1, &copyRegion);
        copyRegion.srcOffset = copy.indexOffset;
        copyRegion.size = mesh.getIndexCount() * sizeof(uint32_t);
        vkCmdCopyBuffer(transferCmd, stagingBuffer.buffer, mesh.getRMesh().indexBuffer.buffer, 1, &copyRegion);
      }

      resultStruct.stagingBuffers.push_back(stagingBuffer);
    }

    resultStruct.resources = std::move(result);

    return std::move(resultStruct);
  }
} // namespace kt::vkh
