#include "keptech/core/rendering/commandBuffer.hpp"
#include "keptech/core/rendering/texture.hpp"
#include "keptech/vulkan/renderer.hpp"

#include "keptech/core/rendering/pipeline.hpp"
#include "keptech/vulkan/buffer.hpp"
#include "keptech/vulkan/commandBuffer.hpp"
#include "keptech/vulkan/helpers/pipeline.hpp"
#include "keptech/vulkan/material.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include "vk-logger.hpp"
#include "vulkan/vulkan.hpp"
#include <algorithm>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/core/components/camera.hpp>
#include <keptech/core/window.hpp>
#include <memory>
#include <utility>
#include <vk_mem_alloc_structs.hpp>

#include "conversions.hpp"

namespace keptech::vkh {
  std::expected<BufPtr, std::string>
  RendererBackend::createBuffer(const BufferCreateInfo& createInfo) {
    vk::BufferCreateInfo bufInfo{
        .size = createInfo.size,
        .usage = from(createInfo.usage) |
                 vk::BufferUsageFlagBits::eShaderDeviceAddress,
        .sharingMode = vk::SharingMode::eExclusive,
    };

    vma::AllocationCreateInfo allocInfo{
        .flags = createInfo.memoryType == BufferMemoryType::CpuToGpu
                     ? vma::AllocationCreateFlagBits::eMapped
                     : vma::AllocationCreateFlags{},
        .usage = from(createInfo.memoryType),
    };

    auto vkRes = AddressedAllocatedBuffer::create(vkcore.device.logical,
                                                  vkcore.allocator, bufInfo,
                                                  allocInfo, createInfo.name);
    if (!vkRes) {
      return std::unexpected(
          fmt::format("Failed to create buffer: {}", vkRes.error()));
    }

#ifndef NDEBUG
    if (createInfo.name.has_value()) {
      VkBuffer vkBuffer = vkRes.value().buffer;
      vkcore.device.logical.setDebugUtilsObjectNameEXT(
          vk::DebugUtilsObjectNameInfoEXT{
              .objectType = vk::ObjectType::eBuffer,
              .objectHandle = reinterpret_cast<uint64_t>(vkBuffer),
              .pObjectName = createInfo.name->c_str(),
          });
    }
#endif

    auto buffer =
        std::make_shared<vkh::Buffer>(vkcore.allocator, vkRes.value());

    return buffer;
  }

  std::expected<PipelinePtr, std::string>
  RendererBackend::createPipeline(PipelineCreateInfo createInfo) {
    GraphicsPipelineConfig config;

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

    VKH_MAKE(shaderModule,
             Shader::create(vkcore.device.logical, createInfo.shader.code),
             "Failed to create shader module");

    for (auto& stage : createInfo.shader.stages) {
      vk::PipelineShaderStageCreateInfo stageInfo{
          .stage = from(stage.stage),
          .module = shaderModule.get(),
          .pName = stage.name, // NOLINT
      };

      shaderStages.push_back(stageInfo);
    }

    config.shaders = shaderStages;

    switch (createInfo.shader.mode) {
    case shaders::RenderingMode::Deferred: {
      createInfo.attachments = deferredPipelineAttachmentConfig();
      break;
    }
    case shaders::RenderingMode::DeferredLighting: {
      createInfo.attachments = {
          .colorFormats = {TextureFormat::RGBA16F, TextureFormat::RGBA16F}};
    } break;
    case shaders::RenderingMode::Forward:
      if (createInfo.attachments.colorFormats.empty()) {
        createInfo.attachments.colorFormats.push_back(
            from(getSwapchainImageFormat(), TextureFormat::RGBA8));
      }
      break;
    case shaders::RenderingMode::Custom:
      break;
    }

    for (auto& colorFormat : createInfo.attachments.colorFormats) {
      config.rendering.colorAttachmentFormats.push_back(
          from(colorFormat, getSwapchainImageFormat()));
      config.blendAttachments.push_back(vk::PipelineColorBlendAttachmentState{
          .blendEnable = createInfo.blend.enableBlending,
          .srcColorBlendFactor = from(createInfo.blend.src),
          .dstColorBlendFactor = from(createInfo.blend.dst),
          .colorBlendOp = vk::BlendOp::eAdd,
          .srcAlphaBlendFactor = vk::BlendFactor::eOne,
          .dstAlphaBlendFactor = vk::BlendFactor::eZero,
          .alphaBlendOp = vk::BlendOp::eAdd,
          .colorWriteMask =
              vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
      });
    }

    config.rendering.depthAttachmentFormat =
        from(createInfo.attachments.depthFormat, vk::Format::eD16Unorm);
    config.rendering.stencilAttachmentFormat =
        from(createInfo.attachments.stencilFormat, vk::Format::eUndefined);

    // Input Assembly
    config.inputAssembly.topology = from(createInfo.topology);

    // Rasterizer
    config.rasterizer.polygonMode = from(createInfo.rasterizer.polygonMode);
    config.rasterizer.cullMode = from(createInfo.rasterizer.cullMode);
    config.rasterizer.frontFace = from(createInfo.rasterizer.frontFace);

    config.depthStencilState.depthTestEnable =
        createInfo.depth.depthCompareOp.has_value();
    config.depthStencilState.depthWriteEnable = createInfo.depth.depthWrite;
    if (createInfo.depth.depthCompareOp.has_value()) {
      config.depthStencilState.depthCompareOp =
          from(createInfo.depth.depthCompareOp.value());
    }

    // Vertex Assembly
    std::vector<vk::VertexInputAttributeDescription> vertexAttributes;
    uint32_t binding = 0;
    for (auto& param : createInfo.shader.vertexLayout) {
      uint32_t voffset = 0;
      uint32_t location = 0;
      for (auto& type : param) {
        vk::VertexInputAttributeDescription attrDesc{
            .location = location++,
            .binding = binding,
            .format = from(type, vk::Format::eUndefined),
            .offset = voffset,
        };
        vertexAttributes.push_back(attrDesc);
        voffset += getSize(type);
      }
      ++binding;
    }

    std::vector<vk::VertexInputBindingDescription> vertexBindings;
    std::ranges::sort(createInfo.layout.vertexInstanceBindings);
    uint32_t currentBinding = 0;
    for (auto& param : createInfo.shader.vertexLayout) {

      size_t bindingStride = 0;
      for (auto& type : param) {
        bindingStride += getSize(type);
      }

      auto isInstance =
          createInfo.layout.vertexInstanceBindings.end() !=
          std::ranges::find(createInfo.layout.vertexInstanceBindings,
                            static_cast<uint32_t>(currentBinding));

      vk::VertexInputBindingDescription bindingDesc{
          .binding = static_cast<uint32_t>(currentBinding++),
          .stride = static_cast<uint32_t>(bindingStride),
          .inputRate = isInstance ? vk::VertexInputRate::eInstance
                                  : vk::VertexInputRate::eVertex,
      };
      vertexBindings.push_back(bindingDesc);
    }

    config.vertexInput.attributes = vertexAttributes;
    config.vertexInput.bindings = vertexBindings;

    // Layout
    for (auto& pushConstant : createInfo.layout.pushConstantRanges) {
      vk::PushConstantRange range{
          .stageFlags = from(pushConstant.stages),
          .offset = pushConstant.offset,
          .size = pushConstant.size,
      };
      config.layout.pushConstantRanges.push_back(range);
    }

    config.layout.setLayouts.push_back(globalDescriptorSets.layout);

    // TODO: User Descriptor sets

    auto vkLayoutInfo = config.layout.build();

    VK_MAKE(pipelineLayout,
            vkcore.device.logical.createPipelineLayout(vkLayoutInfo),
            "Failed to create pipeline layout");

#ifndef NDEBUG
    std::string layoutName =
        fmt::format("{}_pipeline_layout", createInfo.shader.name);

    VkPipelineLayout vkPipelineLayout = *pipelineLayout;
    vkcore.device.logical.setDebugUtilsObjectNameEXT(
        vk::DebugUtilsObjectNameInfoEXT{
            .objectType = vk::ObjectType::ePipelineLayout,
            .objectHandle = reinterpret_cast<uint64_t>(vkPipelineLayout),
            .pObjectName = layoutName.c_str(),
        });
#endif

    auto vkConfig = config.build();
    vkConfig.layout = *pipelineLayout;

    VK_MAKE(pipeline,
            vkcore.device.logical.createGraphicsPipeline(nullptr, vkConfig),
            "Failed to create graphics pipeline");

#ifndef NDEBUG
    VkPipeline vkPipeline = *pipeline;
    vkcore.device.logical.setDebugUtilsObjectNameEXT(
        vk::DebugUtilsObjectNameInfoEXT{
            .objectType = vk::ObjectType::ePipeline,
            .objectHandle = reinterpret_cast<uint64_t>(vkPipeline),
            .pObjectName = createInfo.shader.name.c_str(),
        });
#endif

    PipelineStage stage = PipelineStage::Deferred;
    switch (createInfo.shader.mode) {
    case shaders::RenderingMode::Deferred:
      stage = PipelineStage::Deferred;
      break;
    case shaders::RenderingMode::Forward:
      if (createInfo.blend.enableBlending) {
        stage = PipelineStage::Transparent;
      } else {
        stage = PipelineStage::Opaque;
      }
      break;
    case shaders::RenderingMode::DeferredLighting:
      stage = PipelineStage::DeferredLighting;
      break;
    case shaders::RenderingMode::Custom:
      return std::unexpected("Unsupported shader rendering mode");
      break;
    }

    uint32_t extraInstanceDataSize = 0;
    for (auto& instanceDataType : createInfo.layout.instanceDataTypes) {
      switch (instanceDataType) {
      case shaders::DataType::None:
      case shaders::DataType::Void:
        break;
      case shaders::DataType::Bool:
      case shaders::DataType::I8:
      case shaders::DataType::U8:
        extraInstanceDataSize += 1;
        break;
      case shaders::DataType::F16:
      case shaders::DataType::I16:
      case shaders::DataType::U16:
      case shaders::DataType::I8_2:
      case shaders::DataType::U8_2:
        extraInstanceDataSize += 2;
        break;
      case shaders::DataType::F32:
      case shaders::DataType::I32:
      case shaders::DataType::U32:
      case shaders::DataType::F16_2:
      case shaders::DataType::I16_2:
      case shaders::DataType::U16_2:
      case shaders::DataType::I8_4:
      case shaders::DataType::U8_4:
        extraInstanceDataSize += 4;
        break;
      case shaders::DataType::F64:
      case shaders::DataType::I64:
      case shaders::DataType::U64:
      case shaders::DataType::F32_2:
      case shaders::DataType::F16_4:
      case shaders::DataType::I32_2:
      case shaders::DataType::I16_4:
      case shaders::DataType::U32_2:
      case shaders::DataType::U16_4:
        extraInstanceDataSize += 8;
        break;
      case shaders::DataType::F64_2:
      case shaders::DataType::F32_4:
      case shaders::DataType::I64_2:
      case shaders::DataType::I32_4:
      case shaders::DataType::U64_2:
      case shaders::DataType::U32_4:
        extraInstanceDataSize += 16;
        break;
      case shaders::DataType::F16_3:
      case shaders::DataType::I16_3:
      case shaders::DataType::U16_3:
        extraInstanceDataSize += 6;
        break;
      case shaders::DataType::F32_3:
      case shaders::DataType::I32_3:
      case shaders::DataType::U32_3:
        extraInstanceDataSize += 12;
        break;
      case shaders::DataType::F64_3:
      case shaders::DataType::I64_3:
      case shaders::DataType::U64_3:
        extraInstanceDataSize += 24;
        break;
      case shaders::DataType::F64_4:
      case shaders::DataType::I64_4:
      case shaders::DataType::U64_4:
        extraInstanceDataSize += 32;
        break;
      case shaders::DataType::I8_3:
      case shaders::DataType::U8_3:
        extraInstanceDataSize += 3;
        break;
      case shaders::DataType::F32_4x4:
        extraInstanceDataSize += 64;
        break;
      case shaders::DataType::Sampler2D:
        extraInstanceDataSize += 4; // u32 index
        break;
      }
    }

    LoadedPipeline mat(
        std::move(pipeline), std::move(pipelineLayout), extraInstanceDataSize,
        stage, std::move(createInfo.layout.instanceDataTypes)
#ifdef KT_ADD_RESOURCE_INFO
                   ,
        createInfo.shader.name, createInfo.shader.mode, createInfo
#endif

    );

    VK_DEBUG("Created material '{}'", createInfo.shader.name);

    std::shared_ptr ptr = std::make_shared<LoadedPipeline>(std::move(mat));

    return ptr;
  }

  std::expected<std::vector<ImgPtr>, std::string>
  RendererBackend::createImages(const std::vector<ImageCreateInfo>& infos) {
    std::vector<ImgPtr> images;
    images.reserve(infos.size());

    for (auto& info : infos) {
      if (info.data == nullptr)
        continue;
    }

    std::vector<AddressedAllocatedBuffer> stagingBuffers;
    stagingBuffers.reserve(infos.size());

    for (auto& info : infos) {
      if (info.data == nullptr)
        continue;
      size_t pixelSize = componentsSize(info.format, TextureFormat::RGBA8);

      size_t sz = pixelSize * info.size.x * info.size.y * info.size.z;

      VKH_MAKE(b,
               AddressedAllocatedBuffer::create(
                   vkcore.device.logical, vkcore.allocator,
                   {
                       .size = sz,
                       .usage = vk::BufferUsageFlagBits::eTransferSrc |
                                vk::BufferUsageFlagBits::eShaderDeviceAddress,
                   },
                   {
                       .flags = vma::AllocationCreateFlagBits::eMapped,
                       .usage = vma::MemoryUsage::eCpuToGpu,
                   }),
               "Failed to create staging buffer for image transfer");
      stagingBuffers.emplace_back(b);
    }

    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = frameInfo.perFrame->pools.compute->pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };
    VK_MAKE(cmdBufs, vkcore.device.logical.allocateCommandBuffers(allocInfo),
            "Failed to allocate command buffer for image transfer");
    auto& cmdBuf = cmdBufs[0];

    cmdBuf.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });

    size_t stagingIndex = 0;

    for (auto& info : infos) {
      vk::Format vkFormat = from(info.format, vk::Format::eR8G8B8A8Unorm);

      vk::ImageCreateInfo imageCreateInfo{
          .imageType =
              info.size.z == 1 ? vk::ImageType::e2D : vk::ImageType::e3D,
          .format = vkFormat,
          .extent = {.width = info.size.x,
                     .height = info.size.y,
                     .depth = info.size.z},
          .mipLevels = info.mipLevels,
          .arrayLayers = 1,
          .samples = vk::SampleCountFlagBits::e1,
          .tiling = vk::ImageTiling::eOptimal,
          .usage = from(info.usage),
          .sharingMode = vk::SharingMode::eExclusive,
          .initialLayout = vk::ImageLayout::eUndefined,
      };

      vma::AllocationCreateInfo allocCreateInfo =

          vma::AllocationCreateInfo{
              .usage = vma::MemoryUsage::eGpuOnly,
              .requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal};

      auto imageViewCreateInfo = vk::ImageViewCreateInfo{
          .viewType = info.size.z == 1 ? vk::ImageViewType::e2D
                                       : vk::ImageViewType::e3D,
          .format = imageCreateInfo.format,
          .subresourceRange =
              {
                  .aspectMask = aspectFromFormat(info.format),
                  .baseMipLevel = 0,
                  .levelCount = 1,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
      };

      VKH_MAKE(img,
               AllocatedImage::create(vkcore.allocator, vkcore.device.logical,
                                      imageCreateInfo, allocCreateInfo,
                                      imageViewCreateInfo, true, info.name),
               "Failed to create texture image");

#ifndef NDEBUG
      VkImage vkImage = img.image;
      VkImageView vkImageView = img.view;
      vkcore.device.logical.setDebugUtilsObjectNameEXT(
          vk::DebugUtilsObjectNameInfoEXT{
              .objectType = vk::ObjectType::eImage,
              .objectHandle = reinterpret_cast<uint64_t>(vkImage),
              .pObjectName = info.name.c_str(),
          });

      std::string viewName = info.name + "_view";

      vkcore.device.logical.setDebugUtilsObjectNameEXT(
          vk::DebugUtilsObjectNameInfoEXT{
              .objectType = vk::ObjectType::eImageView,
              .objectHandle = reinterpret_cast<uint64_t>(vkImageView),
              .pObjectName = viewName.c_str(),
          });
#endif

      std::shared_ptr ptr = std::make_shared<vkh::Texture>(
          vkcore.allocator, vkcore.device.logical, img, info.size, info.format,
          info.mipLevels
#ifdef KT_ADD_RESOURCE_INFO
          ,
          info.name, info.usage
#endif
      );

      ptr->setIndex(nextTextureIndex++);
      for (auto& u : textureDescriptorsToUpdate) {
        u.push_back(ptr);
      }

      ImgPtr imgPtr = ptr;
      images.push_back(std::move(imgPtr));

      if (info.data == nullptr) {
        continue;
      }

      size_t pixelSize = componentsSize(info.format, TextureFormat::RGBA8);

      size_t imgSize = pixelSize * info.size.x * info.size.y * info.size.z;

      auto& stagingBuf = stagingBuffers[stagingIndex++];
      memcpy(stagingBuf.mapping(), info.data, imgSize);

      vk::ImageMemoryBarrier2 toWriteBarrier{
          .dstStageMask = vk::PipelineStageFlagBits2::eCopy,
          .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
          .oldLayout = vk::ImageLayout::eUndefined,
          .newLayout = vk::ImageLayout::eTransferDstOptimal,
          .image = ptr->getImage().image,
          .subresourceRange =
              vk::ImageSubresourceRange{
                  .aspectMask = aspectFromFormat(info.format),
                  .baseMipLevel = 0,
                  .levelCount = info.mipLevels,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
      };
      cmdBuf.pipelineBarrier2(vk::DependencyInfo{
          .imageMemoryBarrierCount = 1,
          .pImageMemoryBarriers = &toWriteBarrier,
      });

      vk::BufferImageCopy2 copyRegion{
          .bufferOffset = 0,
          .bufferRowLength = 0,
          .bufferImageHeight = 0,
          .imageSubresource =
              vk::ImageSubresourceLayers{
                  .aspectMask = aspectFromFormat(info.format),
                  .mipLevel = 0,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
          .imageOffset = vk::Offset3D{.x = 0, .y = 0, .z = 0},
          .imageExtent =
              vk::Extent3D{
                  .width = static_cast<uint32_t>(info.size.x),
                  .height = static_cast<uint32_t>(info.size.y),
                  .depth = static_cast<uint32_t>(info.size.z),
              },
      };

      cmdBuf.copyBufferToImage2(vk::CopyBufferToImageInfo2{
          .srcBuffer = stagingBuf.buffer,
          .dstImage = ptr->getImage().image,
          .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
          .regionCount = 1,
          .pRegions = &copyRegion,
      });

      vk::ImageMemoryBarrier2 toShaderReadBarrier{
          .srcStageMask = vk::PipelineStageFlagBits2::eCopy,
          .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
          .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
          .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
          .oldLayout = vk::ImageLayout::eTransferDstOptimal,
          .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
          .image = ptr->getImage().image,
          .subresourceRange =
              vk::ImageSubresourceRange{
                  .aspectMask = aspectFromFormat(info.format),
                  .baseMipLevel = 0,
                  .levelCount = info.mipLevels,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
      };
      cmdBuf.pipelineBarrier2(vk::DependencyInfo{
          .imageMemoryBarrierCount = 1,
          .pImageMemoryBarriers = &toShaderReadBarrier,
      });
    }

    cmdBuf.end();

    std::vector<BufPtr> bufs;
    bufs.reserve(stagingBuffers.size());

    for (auto& stagingBuf : stagingBuffers) {
      auto buffer = std::make_shared<vkh::Buffer>(vkcore.allocator, stagingBuf);
      bufs.push_back(buffer);
    }

    CmdBufPtr cmdBuffer = std::make_unique<vkh::CommandBuffer>(
        std::move(cmdBuf), CmdBufType::Compute);

    std::vector<SubmitInfo> submitInfos;
    submitInfos.push_back({
        .commandBuffer = std::move(cmdBuffer),
        .trackedBuffers = std::move(bufs),
        .trackedTextures = images,
    });

    submitCommandBuffers(std::move(submitInfos));

    return std::move(images);
  }

  std::expected<std::vector<ImgPtr>, std::string>
  RendererBackend::createImages(const std::vector<ImageUploadInfo>& infos) {
    std::vector<ImageCreateInfo> createInfos;
    createInfos.reserve(infos.size());

    for (auto& info : infos) {
      TextureFormat format = TextureFormat::RGBA8;
      switch (info.image->getChannels()) {
      case 1:
        format = TextureFormat::R8;
        break;
      case 2:
        format = TextureFormat::RG8;
        break;
      case 3:
        format = TextureFormat::RGB8;
        break;
      default:;
      }

      ImageCreateInfo createInfo{
          .name = info.name,
          .size = {info.image->getSize().x, info.image->getSize().y, 1.f},
          .format = format,
          .usage = info.usage,
          .mipLevels = info.mipLevels,
          .data = info.image->getData(),
      };
      createInfos.push_back(createInfo);
    }

    return createImages(createInfos);
  }

  std::expected<SamplerPtr, std::string>
  RendererBackend::createSampler(const SamplerCreateInfo& info) {
    vk::SamplerCreateInfo samplerInfo{
        .magFilter = from(info.magFilter),
        .minFilter = from(info.minFilter),
        .mipmapMode = fromMip(info.mipmapFilter),
        .addressModeU = from(info.addressModeU),
        .addressModeV = from(info.addressModeV),
        .addressModeW = from(info.addressModeW),
        .anisotropyEnable = info.enableAnisotropy ? VK_TRUE : VK_FALSE,
        .maxAnisotropy = info.anisotropyLevel,
        .borderColor = vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
    };

    VK_MAKE(sampler, vkcore.device.logical.createSampler(samplerInfo),
            "Failed to create texture sampler");

    return std::make_shared<vkh::Sampler>(std::move(sampler));
  }

  void RendererBackend::textureLayoutTransition(
      const CmdBufPtr& cmdBuf,
      const std::vector<TextureTransition>& transitions) {
    std::vector<vk::ImageMemoryBarrier2> vkBarriers;
    vkBarriers.reserve(transitions.size());

    for (auto& transition : transitions) {
      auto* texture = static_cast<vkh::Texture*>(transition.texture);
      vk::ImageLayout oldLayout = vk::ImageLayout::eUndefined;
      vk::ImageLayout newLayout = vk::ImageLayout::eUndefined;
      vk::PipelineStageFlags2 srcStageMask;
      vk::PipelineStageFlags2 dstStageMask;
      vk::AccessFlags2 srcAccessMask;
      vk::AccessFlags2 dstAccessMask;

      switch (transition.type) {
      case TextureTransitionType::UndefinedToRenderable:
        oldLayout = vk::ImageLayout::eUndefined;
        newLayout = isDepthFormat(texture->getFormat())
                        ? vk::ImageLayout::eDepthAttachmentOptimal
                        : vk::ImageLayout::eColorAttachmentOptimal;
        srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
        dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        srcAccessMask = vk::AccessFlagBits2::eNone;
        dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
        break;
      case TextureTransitionType::RenderableToShaderRead:
        oldLayout = isDepthFormat(texture->getFormat())
                        ? vk::ImageLayout::eDepthAttachmentOptimal
                        : vk::ImageLayout::eColorAttachmentOptimal;
        newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
        srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
        dstAccessMask = vk::AccessFlagBits2::eShaderRead;
        break;
      case TextureTransitionType::ShaderReadToRenderable:
        oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        newLayout = isDepthFormat(texture->getFormat())
                        ? vk::ImageLayout::eDepthAttachmentOptimal
                        : vk::ImageLayout::eColorAttachmentOptimal;
        srcStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
        dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        srcAccessMask = vk::AccessFlagBits2::eShaderRead;
        dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
        break;
      default:
        VK_ERROR("Unknown texture transition type");
        continue;
      }

      vk::ImageMemoryBarrier2 barrier{
          .srcStageMask = srcStageMask,
          .srcAccessMask = srcAccessMask,
          .dstStageMask = dstStageMask,
          .dstAccessMask = dstAccessMask,
          .oldLayout = oldLayout,
          .newLayout = newLayout,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = texture->getImage().image,
          .subresourceRange = vk::ImageSubresourceRange{
              .aspectMask = aspectFromFormat(texture->getFormat()),
              .baseMipLevel = 0,
              .levelCount = texture->getMipLevels(),
              .baseArrayLayer = 0,
              .layerCount = 1,
          }};

      vkBarriers.push_back(barrier);
    }

    vk::raii::CommandBuffer& graphicsCmdBuffer =
        dynamic_cast<vkh::CommandBuffer*>(cmdBuf.get())->get();

    graphicsCmdBuffer.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = static_cast<uint32_t>(vkBarriers.size()),
        .pImageMemoryBarriers = vkBarriers.data(),
    });
  }

  void RendererBackend::loadImGuiImageHandle(ImgPtr& texture) {
    vkh::Texture& vkTexture =
        *std::dynamic_pointer_cast<vkh::Texture>(texture).get();

    auto handle = ImGui_ImplVulkan_AddTexture(
        *imGuiObjects->sampler,
        static_cast<VkImageView>(vkTexture.getImage().view),
        static_cast<VkImageLayout>(vk::ImageLayout::eShaderReadOnlyOptimal));

    texture->setImGuiHandle(handle);
  }
} // namespace keptech::vkh
