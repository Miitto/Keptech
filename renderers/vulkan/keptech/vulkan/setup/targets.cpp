#include "setup.hpp"

#include "keptech/vulkan/constants.hpp"
#include "keptech/vulkan/renderer.hpp"
#include "macros.hpp"
#include <random>

namespace kt::vkh::setup {

  std::expected<RenderTargets, std::string> createRenderTargets(const VulkanCore& vkcore, const Formats& formats,
                                                                const glm::ivec2& framebufferSize) {
    VkExtent3D extent{
        .width = static_cast<uint32_t>(framebufferSize.x),
        .height = static_cast<uint32_t>(framebufferSize.y),
        .depth = 1,
    };
    VkImageCreateInfo imgInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent = extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    };
    VkImageCreateInfo depthImgInfo = imgInfo;
    depthImgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    depthImgInfo.format = formats.render.depth;

    VmaAllocationCreateInfo allocInfo{
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    VkImageViewCreateInfo imgViewInfo{
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
    VkImageViewCreateInfo depthImgViewInfo = imgViewInfo;
    depthImgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    imgInfo.format = formats.render.albedo;
    VKH_MAKE(gbufferAlbedo, Image::create(vkcore.allocator, vkcore.device.logical, imgInfo, allocInfo, imgViewInfo, "GBuffer Albedo", true),
             "Failed to create albedo image for GBuffer.");
    imgInfo.format = formats.render.normal;
    VKH_MAKE(gbufferNormal, Image::create(vkcore.allocator, vkcore.device.logical, imgInfo, allocInfo, imgViewInfo, "GBuffer Normal", true),
             "Failed to create normal image for GBuffer.");
    imgInfo.format = formats.render.emissive;
    VKH_MAKE(gbufferEmissive,
             Image::create(vkcore.allocator, vkcore.device.logical, imgInfo, allocInfo, imgViewInfo, "GBuffer Emissive", true),
             "Failed to create emissive image for GBuffer.");
    imgInfo.format = formats.render.metRought;
    VKH_MAKE(gbufferMetRough,
             Image::create(vkcore.allocator, vkcore.device.logical, imgInfo, allocInfo, imgViewInfo, "GBuffer MetRough", true),
             "Failed to create metrough image for GBuffer.");
    VKH_MAKE(gbufferDepth,
             Image::create(vkcore.allocator, vkcore.device.logical, depthImgInfo, allocInfo, depthImgViewInfo, "GBuffer Depth", true),
             "Failed to create depth image for GBuffer.");

    passes::geometry::Target gBuffer{
        .albedo = {gbufferAlbedo, vkcore.allocator, vkcore.device.logical},
        .normal = {gbufferNormal, vkcore.allocator, vkcore.device.logical},
        .emissive = {gbufferEmissive, vkcore.allocator, vkcore.device.logical},
        .metRough = {gbufferMetRough, vkcore.allocator, vkcore.device.logical},
        .depth = {gbufferDepth, vkcore.allocator, vkcore.device.logical},
    };

    imgInfo.format = formats.render.hdr;
    VKH_MAKE(lightDiffuse,
             Image::create(vkcore.allocator, vkcore.device.logical, imgInfo, allocInfo, imgViewInfo, "Light Buffer Diffuse", true),
             "Failed to create diffuse image for light buffer.");
    VKH_MAKE(lightSpecular,
             Image::create(vkcore.allocator, vkcore.device.logical, imgInfo, allocInfo, imgViewInfo, "Light Buffer Specular", true),
             "Failed to create specular image for light buffer.");
    VKH_MAKE(lightCombined,
             Image::create(vkcore.allocator, vkcore.device.logical, imgInfo, allocInfo, imgViewInfo, "Light Buffer Combined", true),
             "Failed to create combined image for light buffer.");

    imgInfo.format = VK_FORMAT_R8_UNORM;
    VkImageCreateInfo ssaoImgInfo = imgInfo;
    ssaoImgInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;

    VKH_MAKE(ssaoResult, Image::create(vkcore.allocator, vkcore.device.logical, ssaoImgInfo, allocInfo, imgViewInfo, "SSAO Result", true),
             "Failed to create SSAO result image.");
    VKH_MAKE(ssaoBlur, Image::create(vkcore.allocator, vkcore.device.logical, imgInfo, allocInfo, imgViewInfo, "SSAO Blur", true),
             "Failed to create SSAO blur image.");

    imgInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    imgInfo.extent.width = constants::SSAO_NOISE_SIZE;
    imgInfo.extent.height = constants::SSAO_NOISE_SIZE;
    imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VKH_MAKE(ssaoNoise, Image::create(vkcore.allocator, vkcore.device.logical, imgInfo, allocInfo, imgViewInfo, "SSAO Noise", true),
             "Failed to create SSAO noise image.");

    LightBuffer lightBuffer{
        .diffuse = {lightDiffuse, vkcore.allocator, vkcore.device.logical},
        .specular = {lightSpecular, vkcore.allocator, vkcore.device.logical},
        .ssaoResult = {ssaoResult, vkcore.allocator, vkcore.device.logical},
        .ssaoNoise = {ssaoNoise, vkcore.allocator, vkcore.device.logical},
        .ssaoBlur = {ssaoBlur, vkcore.allocator, vkcore.device.logical},
        .combined = {lightCombined, vkcore.allocator, vkcore.device.logical},
    };
    imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

#ifndef NDEBUG
    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT =
        reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(vkcore.device.logical, "vkSetDebugUtilsObjectNameEXT"));

    VkDebugUtilsObjectNameInfoEXT nameInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_IMAGE,
    };

    {
      auto name = [&](VkImage image, const char* imageName) {
        nameInfo.objectHandle = (uint64_t)image; // NOLINT
        nameInfo.pObjectName = imageName;
        vkSetDebugUtilsObjectNameEXT(vkcore.device.logical, &nameInfo);
      };

      name(*gBuffer.albedo, "GBuffer Albedo");
      name(*gBuffer.normal, "GBuffer Normal");
      name(*gBuffer.emissive, "GBuffer Emissive");
      name(*gBuffer.metRough, "GBuffer MetRough");
      name(*gBuffer.depth, "GBuffer Depth");
      name(*lightBuffer.diffuse, "Light Buffer Diffuse");
      name(*lightBuffer.specular, "Light Buffer Specular");
      name(*lightBuffer.ssaoResult, "Light Buffer SSAO Result");
      name(*lightBuffer.ssaoNoise, "Light Buffer SSAO Noise");
      name(*lightBuffer.combined, "Light Buffer Combined");
    }

    {
      auto name = [&](VkImageView view, const char* viewName) {
        nameInfo.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
        nameInfo.objectHandle = (uint64_t)view; // NOLINT
        nameInfo.pObjectName = viewName;
        vkSetDebugUtilsObjectNameEXT(vkcore.device.logical, &nameInfo);
      };

      name(*gBuffer.albedo, "GBuffer Albedo View");
      name(*gBuffer.normal, "GBuffer Normal View");
      name(*gBuffer.emissive, "GBuffer Emissive View");
      name(*gBuffer.metRough, "GBuffer MetRough View");
      name(*gBuffer.depth, "GBuffer Depth View");
      name(*lightBuffer.diffuse, "Light Buffer Diffuse View");
      name(*lightBuffer.specular, "Light Buffer Specular View");
      name(*lightBuffer.ssaoResult, "Light Buffer SSAO Result View");
      name(*lightBuffer.ssaoNoise, "Light Buffer SSAO Noise View");
      name(*lightBuffer.combined, "Light Buffer Combined View");
    }
#endif

    return RenderTargets{
        .gBuffer = std::move(gBuffer),
        .lights = std::move(lightBuffer),
        .framebufferSize = framebufferSize,
    };
  }

  std::expected<void, std::string> writeSsao(const VulkanCore& vkcore, const Buffers& buffers, const RenderTargets& renderTargets) {
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
    constexpr size_t ssaoKernelSize = constants::SSAO_KERNEL_SIZE * sizeof(glm::vec4);
    constexpr size_t ssaoNoiseSize = static_cast<size_t>(constants::SSAO_NOISE_SIZE * constants::SSAO_NOISE_SIZE) * sizeof(glm::vec4);
    std::default_random_engine generator;
    std::vector<glm::vec4> ssaoKernel;
    std::vector<glm::vec4> ssaoNoise;
    ssaoKernel.reserve(constants::SSAO_KERNEL_SIZE);
    ssaoNoise.reserve(16);
    auto lerp = [](float a, float b, float f) { return a + f * (b - a); };
    for (uint32_t i = 0; i < constants::SSAO_KERNEL_SIZE; ++i) {
      glm::vec4 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, 0.f, 1.f);
      sample = glm::normalize(sample);
      sample *= randomFloats(generator);

      float scale = static_cast<float>(i) / static_cast<float>(constants::SSAO_KERNEL_SIZE);
      scale = lerp(0.1f, 1.0f, scale * scale);
      sample *= scale;
      ssaoKernel.push_back(sample);
    }
    for (uint32_t i = 0; i < constants::SSAO_NOISE_SIZE * constants::SSAO_NOISE_SIZE; i++) {
      glm::vec4 noise(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, 0.f, 1.f);
      ssaoNoise.push_back(noise);
    }

    size_t size = ssaoNoiseSize;
    size_t noiseOffset = 0;
    if (!buffers.ssaoKernel->isMapped()) {
      size += ssaoKernelSize;
      noiseOffset = ssaoKernelSize;
    }

    VkBufferCreateInfo bufInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };
    VmaAllocationCreateInfo allocInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
    };
    VKH_MAKE(stagingBuffer, Buffer::create(vkcore.device.logical, vkcore.allocator, bufInfo, allocInfo, "SSAO Noise Staging Buffer"),
             "Failed to create staging buffer for SSAO noise.");

    VkCommandBufferAllocateInfo cmdBufAllocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vkcore.transferPool.pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer cmdBuf{};
    VK_CHECK(vkAllocateCommandBuffers(vkcore.device.logical, &cmdBufAllocInfo, &cmdBuf),
             "Failed to allocate command buffer for SSAO noise.");

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    vkBeginCommandBuffer(cmdBuf, &beginInfo);

    if (buffers.ssaoKernel->isMapped()) {
      memcpy(buffers.ssaoKernel->mapping(), ssaoKernel.data(), ssaoKernelSize);
    } else {
      VkBufferCopy copyRegion{
          .srcOffset = 0,
          .dstOffset = 0,
          .size = ssaoKernelSize,
      };
      memcpy(stagingBuffer.mapping(), ssaoKernel.data(), ssaoKernelSize);
      vkCmdCopyBuffer(cmdBuf, stagingBuffer, *buffers.ssaoKernel, 1, &copyRegion);
    }

    {
      VkImageMemoryBarrier2 barrier{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
          .srcAccessMask = VK_ACCESS_2_NONE,
          .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
          .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .image = *renderTargets.lights.ssaoNoise,
          .subresourceRange =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .baseMipLevel = 0,
                  .levelCount = 1,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
      };
      VkDependencyInfo depInfo{
          .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .imageMemoryBarrierCount = 1,
          .pImageMemoryBarriers = &barrier,
      };
      vkCmdPipelineBarrier2(cmdBuf, &depInfo);
    }

    VkBufferImageCopy region{
        .bufferOffset = noiseOffset,
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
        .imageExtent =
            {
                .width = constants::SSAO_NOISE_SIZE,
                .height = constants::SSAO_NOISE_SIZE,
                .depth = 1,
            },
    };
    memcpy(stagingBuffer.mapping() + noiseOffset, ssaoNoise.data(), ssaoNoiseSize);
    vkCmdCopyBufferToImage(cmdBuf, stagingBuffer, *renderTargets.lights.ssaoNoise, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    {
      VkImageMemoryBarrier2 barrier{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
          .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
          .dstAccessMask = VK_ACCESS_2_NONE,
          .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          .image = *renderTargets.lights.ssaoNoise,
          .subresourceRange =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .baseMipLevel = 0,
                  .levelCount = 1,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
      };
      VkDependencyInfo depInfo{
          .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .imageMemoryBarrierCount = 1,
          .pImageMemoryBarriers = &barrier,
      };
      vkCmdPipelineBarrier2(cmdBuf, &depInfo);
    }

    vkEndCommandBuffer(cmdBuf);

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmdBuf,
    };
    VK_CHECK(vkQueueSubmit(vkcore.queues.transfer.queue, 1, &submitInfo, VK_NULL_HANDLE),
             "Failed to submit command buffer for SSAO noise.");

    vkDeviceWaitIdle(vkcore.device.logical);

    stagingBuffer.destroy(vkcore.allocator);

    return {};
  }
} // namespace kt::vkh::setup
