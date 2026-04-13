#include "setup.hpp"

#include "macros.hpp"

namespace kt::vkh::setup {

  std::expected<Renderer::RenderTargets, std::string> createRenderTargets(const Renderer::VulkanCore& vkcore, const Formats& formats,
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
    VKH_MAKE(gbufferAlbedo,
             AllocatedImage::create(vkcore.allocator, vkcore.device.logical, imgInfo, allocInfo, imgViewInfo, true, "GBuffer Albedo"),
             "Failed to create albedo image for GBuffer.");
    imgInfo.format = formats.render.normal;
    VKH_MAKE(gbufferNormal,
             AllocatedImage::create(vkcore.allocator, vkcore.device.logical, imgInfo, allocInfo, imgViewInfo, true, "GBuffer Normal"),
             "Failed to create normal image for GBuffer.");
    imgInfo.format = formats.render.emissive;
    VKH_MAKE(gbufferEmissive,
             AllocatedImage::create(vkcore.allocator, vkcore.device.logical, imgInfo, allocInfo, imgViewInfo, true, "GBuffer Emissive"),
             "Failed to create emissive image for GBuffer.");
    imgInfo.format = formats.render.metRought;
    VKH_MAKE(gbufferMetRough,
             AllocatedImage::create(vkcore.allocator, vkcore.device.logical, imgInfo, allocInfo, imgViewInfo, true, "GBuffer MetRough"),
             "Failed to create metrough image for GBuffer.");
    VKH_MAKE(
        gbufferDepth,
        AllocatedImage::create(vkcore.allocator, vkcore.device.logical, depthImgInfo, allocInfo, depthImgViewInfo, true, "GBuffer Depth"),
        "Failed to create depth image for GBuffer.");

    GBuffer gBuffer{
        .albedo = gbufferAlbedo,
        .normal = gbufferNormal,
        .emissive = gbufferEmissive,
        .metRough = gbufferMetRough,
        .depth = gbufferDepth,
    };

    imgInfo.format = formats.render.hdr;
    VKH_MAKE(lightDiffuse,
             AllocatedImage::create(vkcore.allocator, vkcore.device.logical, imgInfo, allocInfo, imgViewInfo, true, "Light Buffer Diffuse"),
             "Failed to create diffuse image for light buffer.");
    VKH_MAKE(
        lightSpecular,
        AllocatedImage::create(vkcore.allocator, vkcore.device.logical, imgInfo, allocInfo, imgViewInfo, true, "Light Buffer Specular"),
        "Failed to create specular image for light buffer.");
    VKH_MAKE(
        lightCombined,
        AllocatedImage::create(vkcore.allocator, vkcore.device.logical, imgInfo, allocInfo, imgViewInfo, true, "Light Buffer Combined"),
        "Failed to create combined image for light buffer.");
    LightBuffer lightBuffer{
        .diffuse = lightDiffuse,
        .specular = lightSpecular,
        .combined = lightCombined,
    };

    std::array<Renderer::RenderTargets::BloomMip, constants::BLOOM_MIP_LEVELS> bloomMips{};

    VkImageCreateInfo bloomImgInfo = imgInfo;

    glm::ivec2 mipSize = framebufferSize;
    for (size_t i = 0; i < constants::BLOOM_MIP_LEVELS; i++) {
      mipSize /= 2;

      bloomImgInfo.extent.width = static_cast<uint32_t>(mipSize.x);
      bloomImgInfo.extent.height = static_cast<uint32_t>(mipSize.y);

      VKH_MAKE(bloomMip,
               AllocatedImage::create(vkcore.allocator, vkcore.device.logical, bloomImgInfo, allocInfo, imgViewInfo, true,
                                      "Bloom Mip " + std::to_string(i)),
               "Failed to create bloom mip level");

      bloomMips[i] = Renderer::RenderTargets::BloomMip{
          .image = bloomMip,
          .size = mipSize,
      };
    }

#ifndef NDEBUG
    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT =
        reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(vkcore.device.logical, "vkSetDebugUtilsObjectNameEXT"));

    VkDebugUtilsObjectNameInfoEXT nameInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_IMAGE,
    };

    {
      auto name = [&](VkImage image, const char* imageName) {
        nameInfo.objectHandle = (uint64_t)image;
        nameInfo.pObjectName = imageName;
        vkSetDebugUtilsObjectNameEXT(vkcore.device.logical, &nameInfo);
      };

      name(gBuffer.albedo.image, "GBuffer Albedo");
      name(gBuffer.normal.image, "GBuffer Normal");
      name(gBuffer.emissive.image, "GBuffer Emissive");
      name(gBuffer.metRough.image, "GBuffer MetRough");
      name(gBuffer.depth.image, "GBuffer Depth");
      name(lightBuffer.diffuse.image, "Light Buffer Diffuse");
      name(lightBuffer.specular.image, "Light Buffer Specular");
      name(lightBuffer.combined.image, "Light Buffer Combined");

      for (size_t i = 0; i < bloomMips.size(); i++) {
        name(bloomMips[i].image.image, ("Bloom Mip " + std::to_string(i)).c_str());
      }
    }

    {
      auto name = [&](VkImageView view, const char* viewName) {
        nameInfo.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
        nameInfo.objectHandle = (uint64_t)view;
        nameInfo.pObjectName = viewName;
        vkSetDebugUtilsObjectNameEXT(vkcore.device.logical, &nameInfo);
      };

      name(gBuffer.albedo.view, "GBuffer Albedo View");
      name(gBuffer.normal.view, "GBuffer Normal View");
      name(gBuffer.emissive.view, "GBuffer Emissive View");
      name(gBuffer.metRough.view, "GBuffer MetRough View");
      name(gBuffer.depth.view, "GBuffer Depth View");
      name(lightBuffer.diffuse.view, "Light Buffer Diffuse View");
      name(lightBuffer.specular.view, "Light Buffer Specular View");
      name(lightBuffer.combined.view, "Light Buffer Combined View");
      for (size_t i = 0; i < bloomMips.size(); i++) {
        name(bloomMips[i].image.view, ("Bloom Mip " + std::to_string(i) + " View").c_str());
      }
    }
#endif

    return Renderer::RenderTargets{
        .gBuffer = gBuffer,
        .lights = lightBuffer,
        .framebufferSize = framebufferSize,
        .bloomMips = bloomMips,
    };
  }
} // namespace kt::vkh::setup
