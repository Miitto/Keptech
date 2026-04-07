#include "keptech/vulkan/renderer.hpp"

#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
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

    VKH_MAKE(meshes, uploadMeshes(gltfData.meshes), "Failed to upload meshes from glTF data");

    VKH_MAKE(images, createImages(gltfData.images, gltfData), "Failed to create images from glTF data");

    gltf::Scene scene{};
    scene.roots.reserve(gltfData.roots.size());

    for (const auto& node : gltfData.roots) {
      scene.roots.push_back(createNode(node, meshes));
    }

    return scene;
  }

  std::expected<std::vector<Texture>, std::string> Renderer::createImages(const std::vector<fastgltf::Image>& gltfImages,
                                                                          const gltf::Data& gltf) {
    struct Tex {
      ktxTexture* ktx;
      size_t size;
      uint8_t* data;
    };
    std::vector<Tex> textures(gltfImages.size());
    constexpr ktxTextureCreateFlags createFlags = 0;
    auto visitor = fastgltf::visitor{
        [](auto& arg) -> ktxTexture* {
          VK_CRITICAL("Unsupported glTF image source type {}", typeid(arg).name());
          abort();
        },
        [&](fastgltf::sources::Array& array) {
          ktxTexture* ktxTexture = nullptr;
          ktxTexture_CreateFromMemory(reinterpret_cast<const uint8_t*>(array.bytes.data()), array.bytes.size(), createFlags, &ktxTexture);
          return ktxTexture;
        },
        [&](fastgltf::sources::URI& filePath) {
          assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
          assert(filePath.uri.isLocalPath());   // We're only capable of
                                                // loading local files.

          const std::string path(filePath.uri.path().begin(), filePath.uri.path().end());
          ktxTexture* ktxTexture = nullptr;
          ktxTexture_CreateFromNamedFile(path.c_str(), createFlags, &ktxTexture);
          return ktxTexture;
        },
        [&](fastgltf::sources::Vector& vector) {
          ktxTexture* ktxTexture = nullptr;
          ktxTexture_CreateFromMemory(reinterpret_cast<const uint8_t*>(vector.bytes.data()), vector.bytes.size(), createFlags, &ktxTexture);
          return ktxTexture;
        },
        [&](fastgltf::sources::BufferView view) {
          auto& bufferView = gltf.bufferViews[view.bufferViewIndex];
          auto& buffer = gltf.buffers[bufferView.bufferIndex];

          return std::visit(fastgltf::visitor{
                                // We only care about VectorWithMime here, because
                                // we
                                // specify LoadExternalBuffers, meaning all buffers
                                // are already loaded into a vector.
                                [](auto& arg) -> ktxTexture* {
                                  VK_CRITICAL("Unsupported glTF image source buffer "
                                              "type {}",
                                              typeid(arg).name());
                                  abort();
                                },
                                [&](fastgltf::sources::Array& array) {
                                  ktxTexture* ktxTexture = nullptr;
                                  ktxTexture_CreateFromMemory(reinterpret_cast<const uint8_t*>(array.bytes.data()) + bufferView.byteOffset,
                                                              bufferView.byteLength, createFlags, &ktxTexture);
                                  return ktxTexture;
                                },
                                [&](fastgltf::sources::Vector& vector) {
                                  ktxTexture* ktxTexture = nullptr;
                                  ktxTexture_CreateFromMemory(reinterpret_cast<const uint8_t*>(vector.bytes.data()) + bufferView.byteOffset,
                                                              bufferView.byteLength, createFlags, &ktxTexture);
                                  return ktxTexture;
                                },
                            },
                            buffer.data);
        },
    };
    auto enumView = std::views::enumerate(gltfImages);
    std::for_each(std::execution::par, enumView.begin(), enumView.end(), [&](const std::tuple<size_t, fastgltf::Image&>& pair) {
      const auto& [idx, img] = pair;

      auto ktxTexture = std::visit(visitor, img.data);
      textures[idx] = Tex{
          .ktx = ktxTexture,
          .size = ktxTexture_GetImageSize(ktxTexture, 0),
          .data = ktxTexture_GetData(ktxTexture),
      };
    });

    size_t totalSize = std::accumulate(textures.begin(), textures.end(), 0ull, [](size_t sum, const Tex& tex) { return sum + tex.size; });

    AllocatedBuffer stagingBuffer;
    VkCommandBuffer transferCmd = nullptr;
    {
      VkBufferCreateInfo stagingBufferCreateInfo{
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .size = totalSize,
          .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      };

      VmaAllocationCreateInfo stagingAllocInfo{
          .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
          .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
      };

      VKH_MAKE(buffer, AllocatedBuffer::create(m.vkcore.allocator, m.vkcore.device.logical, stagingBufferCreateInfo, stagingAllocInfo),
               "Failed to create staging buffer for image upload");
      stagingBuffer = buffer;

      VkCommandBufferAllocateInfo cmdAllocInfo{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          .commandPool = m.vkcore.transferPool.pool,
          .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          .commandBufferCount = 1,
      };
      VkCommandBufferBeginInfo cmdBeginInfo{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
          .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      };
      VK_CHECK(vkAllocateCommandBuffers(m.vkcore.device.logical, &cmdAllocInfo, &transferCmd),
               "Failed to allocate command buffer for image upload");
      vkBeginCommandBuffer(transferCmd, &cmdBeginInfo);
    }

    VkDeviceSize offset = 0;
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

    for (const auto& tex : textures) {
      memcpy(stagingBuffer.mapping() + offset, tex.data, tex.size);

      VkFormat format = ktxTexture_GetVkFormat(tex.ktx);
      imageCreateInfo.format = format;
      imageCreateInfo.extent = VkExtent3D{
          .width = tex.ktx->baseWidth,
          .height = tex.ktx->baseHeight,
          .depth = 1,
      };

      VKH_MAKE(
          image,
          AllocatedImage::create(m.vkcore.allocator, m.vkcore.device.logical, imageCreateInfo, imageAllocInfo, imageViewCreateInfo, true),
          "Failed to create image for texture");

      auto index = m.nextTextureIndex++;
      result.emplace_back(glm::ivec3{tex.ktx->baseWidth, tex.ktx->baseHeight, 1}, 1, format, index);

      m.loadedTextures.push_back(image);

      for (auto& frame : m.vkcore.perFrame) {
        frame.texToUpdate.emplace_back(image, index);
      }

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

      vkCmdCopyBufferToImage(transferCmd, stagingBuffer.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
      offset += tex.size;

      ktxTexture_Destroy(tex.ktx);
    }

    vkEndCommandBuffer(transferCmd);

    VkFence transferFence = nullptr;
    VkFenceCreateInfo fenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    VK_CHECK(vkCreateFence(m.vkcore.device.logical, &fenceCreateInfo, nullptr, &transferFence), "Failed to create fence for image upload");
    VkCommandBufferSubmitInfo cmdInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = transferCmd,
    };
    VkSubmitInfo2 submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdInfo,
    };
    vkQueueSubmit2(m.vkcore.queues.transfer.queue, 1, &submitInfo, transferFence);

    vkWaitForFences(m.vkcore.device.logical, 1, &transferFence, VK_TRUE, UINT64_MAX);

    return result;
  }

  std::expected<std::vector<Texture>, std::string> Renderer::createImages(const std::vector<ImageUploadInfo>& infos) {
    // TODO: Impl
    return {};
  }

  std::expected<std::vector<Mesh>, std::string> Renderer::uploadMeshes(const std::vector<gltf::MeshData>& meshes) {
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
               AddressedAllocatedBuffer::create(m.vkcore.device.logical, m.vkcore.allocator, vertexBufferCreateInfo, allocInfo),
               "Failed to create vertex buffer for mesh");
      m.loadedBuffers.push_back(vertexBuffer.downcast());
      VKH_MAKE(indexBuffer, AddressedAllocatedBuffer::create(m.vkcore.device.logical, m.vkcore.allocator, indexBufferCreateInfo, allocInfo),
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
               AddressedAllocatedBuffer::create(m.vkcore.device.logical, m.vkcore.allocator, stagingBufferCreateInfo, stagingAllocInfo),
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
      vkEndCommandBuffer(transferCmd);

      VkCommandBufferSubmitInfo cmdInfo{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
          .commandBuffer = transferCmd,
      };

      VkSubmitInfo2 submitInfo{
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          .commandBufferInfoCount = 1,
          .pCommandBufferInfos = &cmdInfo,
      };
      VkFence transferFence = nullptr;
      VkFenceCreateInfo fenceCreateInfo{
          .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      };
      VK_CHECK(vkCreateFence(m.vkcore.device.logical, &fenceCreateInfo, nullptr, &transferFence), "Failed to create fence for mesh upload");
      vkQueueSubmit2(m.vkcore.queues.transfer.queue, 1, &submitInfo, transferFence);
      while (vkWaitForFences(m.vkcore.device.logical, 1, &transferFence, VK_TRUE, UINT64_MAX) == VK_TIMEOUT) {
        std::this_thread::yield(); // Shouldn't happen, but just in case, we don't want to busy wait
      }
      stagingBuffer.destroy(m.vkcore.allocator);
      vkFreeCommandBuffers(m.vkcore.device.logical, m.vkcore.transferPool.pool, 1, &transferCmd);
      vkDestroyFence(m.vkcore.device.logical, transferFence, nullptr);
    }

    return result;
  }
} // namespace kt::vkh
