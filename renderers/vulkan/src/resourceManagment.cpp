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
    case shaders::RenderingMode::Forward:
      // TODO: Implement forward / transparent attechment defaults
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

    vk::DeviceSize offset = 0;
    if (createInfo.layout.useVertexBuffer) {
      offset += sizeof(vk::DeviceAddress);
    }
    if (createInfo.layout.useModelMatrix) {
      offset += sizeof(vk::DeviceAddress) * 2;
    }
    if (offset != 0) {
      auto pcSet = std::ranges::find_if(
          config.layout.pushConstantRanges,
          [](const vk::PushConstantRange& range) {
            return (range.stageFlags | vk::ShaderStageFlagBits::eVertex) !=
                   vk::ShaderStageFlags{};
          });

      if (pcSet == config.layout.pushConstantRanges.end()) {
        vk::PushConstantRange range{
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .offset = 0,
            .size = static_cast<uint32_t>(offset),
        };
        config.layout.pushConstantRanges.push_back(range);
      } else {
        pcSet->size += static_cast<uint32_t>(offset);
        ++pcSet;
        for (; pcSet != config.layout.pushConstantRanges.end(); ++pcSet) {
          pcSet->offset += static_cast<uint32_t>(offset);
        }
      }
    }

    config.layout.setLayouts.push_back(globalDescriptorSets.layout);

    // TODO: User Descriptor sets

    auto vkLayoutInfo = config.layout.build();

    VK_MAKE(pipelineLayout,
            vkcore.device.logical.createPipelineLayout(vkLayoutInfo),
            "Failed to create pipeline layout");

    auto vkConfig = config.build();
    vkConfig.layout = *pipelineLayout;

    VK_MAKE(pipeline,
            vkcore.device.logical.createGraphicsPipeline(nullptr, vkConfig),
            "Failed to create graphics pipeline");

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

    VkPipeline vkPipeline = *pipeline;
    vkcore.device.logical.setDebugUtilsObjectNameEXT(
        vk::DebugUtilsObjectNameInfoEXT{
            .objectType = vk::ObjectType::ePipeline,
            .objectHandle = reinterpret_cast<uint64_t>(vkPipeline),
            .pObjectName = createInfo.shader.name,
        });
#endif

    LoadedPipeline mat{
        .pipeline = std::move(pipeline),
        .pipelineLayout = std::move(pipelineLayout),
    };

#ifdef KT_ADD_RESOURCE_INFO
    mat.setName(createInfo.shader.name);
    mat.setRenderingMode(createInfo.shader.mode);
#endif
    if (createInfo.shader.mode == shaders::RenderingMode::Deferred) {
      mat.setStage(PipelineStage::Deferred);
    } else if (createInfo.shader.mode == shaders::RenderingMode::Forward) {
      if (createInfo.blend.enableBlending) {
        mat.setStage(PipelineStage::Transparent);
      } else {
        mat.setStage(PipelineStage::Opaque);
      }
    } else {
      return std::unexpected("Unsupported shader rendering mode");
    }

    mat.extraInstanceDataSize = 0;
    for (auto& instanceDataType : createInfo.layout.instanceDataTypes) {
      switch (instanceDataType) {
      case shaders::DataType::None:
      case shaders::DataType::Void:
        break;
      case shaders::DataType::Bool:
      case shaders::DataType::I8:
      case shaders::DataType::U8:
        mat.extraInstanceDataSize += 1;
        break;
      case shaders::DataType::F16:
      case shaders::DataType::I16:
      case shaders::DataType::U16:
      case shaders::DataType::I8_2:
      case shaders::DataType::U8_2:
        mat.extraInstanceDataSize += 2;
        break;
      case shaders::DataType::F32:
      case shaders::DataType::I32:
      case shaders::DataType::U32:
      case shaders::DataType::F16_2:
      case shaders::DataType::I16_2:
      case shaders::DataType::U16_2:
      case shaders::DataType::I8_4:
      case shaders::DataType::U8_4:
        mat.extraInstanceDataSize += 4;
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
        mat.extraInstanceDataSize += 8;
        break;
      case shaders::DataType::F64_2:
      case shaders::DataType::F32_4:
      case shaders::DataType::I64_2:
      case shaders::DataType::I32_4:
      case shaders::DataType::U64_2:
      case shaders::DataType::U32_4:
        mat.extraInstanceDataSize += 16;
        break;
      case shaders::DataType::F16_3:
      case shaders::DataType::I16_3:
      case shaders::DataType::U16_3:
        mat.extraInstanceDataSize += 6;
        break;
      case shaders::DataType::F32_3:
      case shaders::DataType::I32_3:
      case shaders::DataType::U32_3:
        mat.extraInstanceDataSize += 12;
        break;
      case shaders::DataType::F64_3:
      case shaders::DataType::I64_3:
      case shaders::DataType::U64_3:
        mat.extraInstanceDataSize += 24;
        break;
      case shaders::DataType::F64_4:
      case shaders::DataType::I64_4:
      case shaders::DataType::U64_4:
        mat.extraInstanceDataSize += 32;
        break;
      case shaders::DataType::I8_3:
      case shaders::DataType::U8_3:
        mat.extraInstanceDataSize += 3;
        break;
      case shaders::DataType::F32_4x4:
        mat.extraInstanceDataSize += 64;
        break;
      case shaders::DataType::Sampler2D:
        mat.extraInstanceDataSize += 4; // u32 index
        break;
      }
    }

    mat.getInstanceDataTypes() = std::move(createInfo.layout.instanceDataTypes);

    VK_INFO("Created material '{}'", createInfo.shader.name);

    std::shared_ptr ptr = std::make_shared<LoadedPipeline>(std::move(mat));

    return ptr;
  }

  std::expected<TexPtr, std::string> RendererBackend::createTexture(
      std::string name, glm::uvec3 size, TextureFormat format,
      Bitflag<TextureUsage> usage, uint32_t mipLevels, bool cpuAccess,
      const void* data) {
    vk::Format vkFormat = from(format, vk::Format::eR8G8B8A8Unorm);

    vk::ImageCreateInfo imageCreateInfo{
        .imageType = size.z == 1 ? vk::ImageType::e2D : vk::ImageType::e3D,
        .format = vkFormat,
        .extent = {.width = size.x, .height = size.y, .depth = size.z},
        .mipLevels = mipLevels,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling =
            cpuAccess ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal,
        .usage = from(usage),
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    vma::AllocationCreateInfo allocCreateInfo =
        cpuAccess
            ? vma::AllocationCreateInfo{.usage = vma::MemoryUsage::eCpuToGpu}
            : vma::AllocationCreateInfo{
                  .usage = vma::MemoryUsage::eGpuOnly,
                  .requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal};

    auto imageViewCreateInfo = vk::ImageViewCreateInfo{
        .viewType =
            size.z == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::e3D,
        .format = imageCreateInfo.format,
        .subresourceRange =
            {
                .aspectMask = aspectFromFormat(format),
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    VKH_MAKE(img,
             AllocatedImage::create(vkcore.allocator, vkcore.device.logical,
                                    imageCreateInfo, allocCreateInfo,
                                    imageViewCreateInfo, true),
             "Failed to create texture image");

#ifndef NDEBUG
    VkImage vkImage = img.image;
    VkImageView vkImageView = img.view;
    vkcore.device.logical.setDebugUtilsObjectNameEXT(
        vk::DebugUtilsObjectNameInfoEXT{
            .objectType = vk::ObjectType::eImage,
            .objectHandle = reinterpret_cast<uint64_t>(vkImage),
            .pObjectName = name.c_str(),
        });

    std::string viewName = name + "_view";

    vkcore.device.logical.setDebugUtilsObjectNameEXT(
        vk::DebugUtilsObjectNameInfoEXT{
            .objectType = vk::ObjectType::eImageView,
            .objectHandle = reinterpret_cast<uint64_t>(vkImageView),
            .pObjectName = viewName.c_str(),
        });
#endif

    vkh::Texture texture(vkcore.allocator, vkcore.device.logical, img, size,
                         format, mipLevels
#ifdef KT_ADD_RESOURCE_INFO
                         ,
                         name, usage
#endif
    );

    return std::make_shared<vkh::Texture>(std::move(texture));
  }

  std::expected<TexPtr, std::string>
  RendererBackend::createTexture(std::string name, const Image& image,
                                 Bitflag<TextureUsage> usage,
                                 uint32_t mipLevels, bool cpuAccess) {
    TextureFormat format = TextureFormat::RGBA8;

    switch (image.getChannels()) {
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

    auto texRes =
        createTexture(name,
                      glm::uvec3{static_cast<uint32_t>(image.getSize().x),
                                 static_cast<uint32_t>(image.getSize().y), 1},
                      format, usage, mipLevels, cpuAccess, image.getData());
    if (!texRes) {
      return std::unexpected(fmt::format(
          "Failed to create texture from image: {}", texRes.error()));
    }
    TexPtr texPtr = std::move(texRes.value());
    auto vkTexPtr = std::static_pointer_cast<vkh::Texture>(texPtr);

    vkh::Texture& texture = *vkTexPtr;

    texture.setIndex(nextTextureIndex++);

    for (auto& u : textureDescriptorsToUpdate) {
      u.push_back(vkTexPtr);
    }

    size_t pixelSize =
        static_cast<size_t>(image.getChannels()) * (image.isHdr() ? 4 : 1);

    size_t expectedSize = pixelSize * image.getSize().x * image.getSize().y;

    auto bufRes = AddressedAllocatedBuffer::create(
        vkcore.device.logical, vkcore.allocator,
        {
            .size = expectedSize,
            .usage = vk::BufferUsageFlagBits::eTransferSrc |
                     vk::BufferUsageFlagBits::eShaderDeviceAddress,
        },
        {
            .flags = vma::AllocationCreateFlagBits::eMapped,
            .usage = vma::MemoryUsage::eCpuToGpu,
        });
    if (!bufRes) {
      return std::unexpected(
          fmt::format("Failed to create staging buffer for image transfer: {}",
                      bufRes.error()));
    }

    memcpy(bufRes.value().mapping(), image.getData(), expectedSize);

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

    vk::ImageMemoryBarrier2 toWriteBarrier{
        .dstStageMask = vk::PipelineStageFlagBits2::eCopy,
        .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eTransferDstOptimal,
        .image = texture.getImage().image,
        .subresourceRange =
            vk::ImageSubresourceRange{
                .aspectMask = aspectFromFormat(texture.getFormat()),
                .baseMipLevel = 0,
                .levelCount = texture.getMipLevels(),
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
                .aspectMask = aspectFromFormat(texture.getFormat()),
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .imageOffset = vk::Offset3D{.x = 0, .y = 0, .z = 0},
        .imageExtent =
            vk::Extent3D{
                .width = static_cast<uint32_t>(texture.getSize().x),
                .height = static_cast<uint32_t>(texture.getSize().y),
                .depth = static_cast<uint32_t>(texture.getSize().z),
            },
    };

    cmdBuf.copyBufferToImage2(vk::CopyBufferToImageInfo2{
        .srcBuffer = bufRes.value().buffer,
        .dstImage = texture.getImage().image,
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
        .image = texture.getImage().image,
        .subresourceRange =
            vk::ImageSubresourceRange{
                .aspectMask = aspectFromFormat(texture.getFormat()),
                .baseMipLevel = 0,
                .levelCount = texture.getMipLevels(),
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    cmdBuf.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &toShaderReadBarrier,
    });

    cmdBuf.end();

    BufPtr stagingBuffer =
        std::make_shared<vkh::Buffer>(vkcore.allocator, bufRes.value());

    CmdBufPtr cmdBuffer = std::make_unique<vkh::CommandBuffer>(
        std::move(cmdBuf), CmdBufType::Compute);

    std::vector<SubmitInfo> infos;
    infos.push_back({
        .commandBuffer = std::move(cmdBuffer),
        .trackedBuffers = {stagingBuffer},
        .trackedTextures = {texPtr},
    });

    submitCommandBuffers(std::move(infos));

    return texPtr;
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

  void RendererBackend::loadImGuiImageHandle(TexPtr& texture) {
    vkh::Texture& vkTexture =
        *std::dynamic_pointer_cast<vkh::Texture>(texture).get();

    auto handle = ImGui_ImplVulkan_AddTexture(
        *imGuiObjects->sampler,
        static_cast<VkImageView>(vkTexture.getImage().view),
        static_cast<VkImageLayout>(vk::ImageLayout::eShaderReadOnlyOptimal));

    texture->setImGuiHandle(handle);
  }
} // namespace keptech::vkh
