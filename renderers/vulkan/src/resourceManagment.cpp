#include "keptech/core/rendering/commandBuffer.hpp"
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

namespace keptech::vkh {
  namespace {
    vk::ShaderStageFlagBits from(shaders::ShaderStages stages) {
      switch (stages) {
      case shaders::ShaderStages::Vertex:
        return vk::ShaderStageFlagBits::eVertex;
      case shaders::ShaderStages::Fragment:
        return vk::ShaderStageFlagBits::eFragment;
      case shaders::ShaderStages::Compute:
        return vk::ShaderStageFlagBits::eCompute;
      }
    }

    vk::ShaderStageFlags from(Bitflag<shaders::ShaderStages> stages) {
      using S = shaders::ShaderStages;
      vk::ShaderStageFlags flags = {};
      if (stages.has(S::Vertex)) {
        flags = flags | vk::ShaderStageFlagBits::eVertex;
      }
      if (stages.has(S::Fragment)) {
        flags = flags | vk::ShaderStageFlagBits::eFragment;
      }
      if (stages.has(S::Compute)) {
        flags = flags | vk::ShaderStageFlagBits::eCompute;
      }

      return flags;
    }

    vk::Format from(TextureFormat format, vk::Format defaultFormat) {
      using Format = TextureFormat;
      switch (format) {
      case Format::RGB8:
        return vk::Format::eR8G8B8Unorm;
      case Format::RGBA8:
        return vk::Format::eR8G8B8A8Unorm;
      case Format::Default:
        return defaultFormat;
      case Format::Undefined:
        return vk::Format::eUndefined;
      case Format::R8:
        return vk::Format::eR8Unorm;
      case Format::R16F:
        return vk::Format::eR16Sfloat;
      case Format::R32F:
        return vk::Format::eR32Sfloat;
      case Format::RG8:
        return vk::Format::eR8G8Unorm;
      case Format::RG16F:
        return vk::Format::eR16G16Sfloat;
      case Format::RG32F:
        return vk::Format::eR32G32Sfloat;
      case Format::RGB16F:
        return vk::Format::eR16G16B16Sfloat;
      case Format::RGB32F:
        return vk::Format::eR32G32B32Sfloat;
      case Format::RGBA16F:
        return vk::Format::eR16G16B16A16Sfloat;
      case Format::RGBA32F:
        return vk::Format::eR32G32B32A32Sfloat;
      case Format::Depth16:
        return vk::Format::eD16Unorm;
      case Format::Depth24Stencil8:
        return vk::Format::eD24UnormS8Uint;
      }
    }

    vk::ImageUsageFlags from(Bitflag<TextureUsage> usage) {
      using Usage = TextureUsage;

      vk::ImageUsageFlags flags = {};

      if (usage.has(Usage::Sampled)) {
        flags = flags | vk::ImageUsageFlagBits::eSampled;
      }
      if (usage.has(Usage::RenderTarget)) {
        flags = flags | vk::ImageUsageFlagBits::eColorAttachment;
      }
      if (usage.has(Usage::Storage)) {
        flags = flags | vk::ImageUsageFlagBits::eStorage;
      }
      if (usage.has(Usage::DepthStencil)) {
        flags = flags | vk::ImageUsageFlagBits::eDepthStencilAttachment;
      }
      if (usage.has(Usage::TransferSrc)) {
        flags = flags | vk::ImageUsageFlagBits::eTransferSrc;
      }
      if (usage.has(Usage::TransferDst)) {
        flags = flags | vk::ImageUsageFlagBits::eTransferDst;
      }
      return flags;
    }

    vk::PrimitiveTopology from(Topology topology) {
      switch (topology) {
      case Topology::TriangleList:
        return vk::PrimitiveTopology::eTriangleList;
      case Topology::TriangleStrip:
        return vk::PrimitiveTopology::eTriangleStrip;
      case Topology::LineList:
        return vk::PrimitiveTopology::eLineList;
      case Topology::LineStrip:
        return vk::PrimitiveTopology::eLineStrip;
      case Topology::PointList:
        return vk::PrimitiveTopology::ePointList;
      default:
        return vk::PrimitiveTopology::eTriangleList;
      }
    }

    vk::PolygonMode from(PolygonMode mode) {
      switch (mode) {
      case PolygonMode::Fill:
        return vk::PolygonMode::eFill;
      case PolygonMode::Line:
        return vk::PolygonMode::eLine;
      case PolygonMode::Point:
        return vk::PolygonMode::ePoint;
      default:
        return vk::PolygonMode::eFill;
      }
    }

    vk::CullModeFlags from(CullMode mode) {
      switch (mode) {
      case CullMode::None:
        return vk::CullModeFlagBits::eNone;
      case CullMode::Front:
        return vk::CullModeFlagBits::eFront;
      case CullMode::Back:
        return vk::CullModeFlagBits::eBack;
      case CullMode::FrontAndBack:
        return vk::CullModeFlagBits::eFrontAndBack;
      default:
        return vk::CullModeFlagBits::eNone;
      }
    }

    vk::FrontFace from(FrontFace face) {
      switch (face) {
      case FrontFace::Clockwise:
        return vk::FrontFace::eClockwise;
      case FrontFace::CounterClockwise:
        return vk::FrontFace::eCounterClockwise;
      default:
        return vk::FrontFace::eClockwise;
      }
    }

    vk::BlendFactor from(BlendFactor factor) {
      switch (factor) {
      case BlendFactor::Zero:
        return vk::BlendFactor::eZero;
      case BlendFactor::One:
        return vk::BlendFactor::eOne;
      case BlendFactor::SrcAlpha:
        return vk::BlendFactor::eSrcAlpha;
      case BlendFactor::OneMinusSrcAlpha:
        return vk::BlendFactor::eOneMinusSrcAlpha;
      default:
        return vk::BlendFactor::eOne;
      }
    }

    vk::CompareOp from(DepthCompareOp op) {
      switch (op) {
      case DepthCompareOp::Never:
        return vk::CompareOp::eNever;
      case DepthCompareOp::Less:
        return vk::CompareOp::eLess;
      case DepthCompareOp::Equal:
        return vk::CompareOp::eEqual;
      case DepthCompareOp::LessEqual:
        return vk::CompareOp::eLessOrEqual;
      case DepthCompareOp::Greater:
        return vk::CompareOp::eGreater;
      case DepthCompareOp::NotEqual:
        return vk::CompareOp::eNotEqual;
      case DepthCompareOp::GreaterEqual:
        return vk::CompareOp::eGreaterOrEqual;
      case DepthCompareOp::Always:
        return vk::CompareOp::eAlways;
      default:
        return vk::CompareOp::eLess;
      }
    }

    vk::BufferUsageFlags from(Bitflag<BufferUsage> usage) {
      using Usage = BufferUsage;

      vk::BufferUsageFlags flags = {};

      if (usage.has(Usage::Vertex)) {
        flags = flags | vk::BufferUsageFlagBits::eVertexBuffer;
      }
      if (usage.has(Usage::Index)) {
        flags = flags | vk::BufferUsageFlagBits::eIndexBuffer;
      }
      if (usage.has(Usage::Uniform)) {
        flags = flags | vk::BufferUsageFlagBits::eUniformBuffer;
      }
      if (usage.has(Usage::Storage)) {
        flags = flags | vk::BufferUsageFlagBits::eStorageBuffer;
      }
      if (usage.has(Usage::TransferSrc)) {
        flags = flags | vk::BufferUsageFlagBits::eTransferSrc;
      }
      if (usage.has(Usage::TransferDst)) {
        flags = flags | vk::BufferUsageFlagBits::eTransferDst;
      }
      return flags;
    }

    vma::MemoryUsage from(BufferMemoryType memoryType) {
      switch (memoryType) {
      case BufferMemoryType::GpuOnly:
        return vma::MemoryUsage::eGpuOnly;
      case BufferMemoryType::CpuToGpu:
        return vma::MemoryUsage::eCpuToGpu;
      case BufferMemoryType::GpuToCpu:
        return vma::MemoryUsage::eGpuToCpu;
      }
    }

    vk::ImageAspectFlags aspectFromFormat(TextureFormat format) {
      vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
      switch (format) {
      case TextureFormat::Depth16:
        aspectMask = vk::ImageAspectFlagBits::eDepth;
        break;
      case TextureFormat::Depth24Stencil8:
        aspectMask =
            vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        break;
      default:
        break;
      }

      return aspectMask;
    }
  } // namespace

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

    auto vkRes = AddressedAllocatedBuffer::create(
        vkcore.device.logical, vkcore.allocator, bufInfo, allocInfo);
    if (!vkRes) {
      return std::unexpected(
          fmt::format("Failed to create buffer: {}", vkRes.error()));
    }

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
      case InstanceDataType::TextureIndex:
        mat.extraInstanceDataSize += sizeof(uint32_t);
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
    vk::ImageCreateInfo imageCreateInfo{
        .imageType = size.z == 1 ? vk::ImageType::e2D : vk::ImageType::e3D,
        .format = from(format, vk::Format::eR8G8B8A8Unorm),
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

    vkh::Texture texture(vkcore.allocator, vkcore.device.logical, img, size,
                         format, mipLevels
#ifdef KT_ADD_RESOURCE_INFO
                         ,
                         name, usage
#endif
    );

    return std::make_shared<vkh::Texture>(std::move(texture));
  }

  void RendererBackend::textureLayoutTransition(
      const CmdBufPtr& cmdBuf,
      const std::vector<TextureTransition>& transitions) {
    std::vector<vk::ImageMemoryBarrier2> vkBarriers;
    vkBarriers.reserve(transitions.size());

    for (auto& transition : transitions) {
      auto* texture = dynamic_cast<vkh::Texture*>(transition.texture);
      vk::ImageLayout oldLayout = vk::ImageLayout::eUndefined;
      vk::ImageLayout newLayout = vk::ImageLayout::eUndefined;
      vk::PipelineStageFlags2 srcStageMask;
      vk::PipelineStageFlags2 dstStageMask;
      vk::AccessFlags2 srcAccessMask;
      vk::AccessFlags2 dstAccessMask;

      switch (transition.type) {
      case TextureTransitionType::UndefinedToRenderable:
        oldLayout = vk::ImageLayout::eUndefined;
        newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
        dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        srcAccessMask = vk::AccessFlagBits2::eNone;
        dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
        break;
      case TextureTransitionType::RenderableToShaderRead:
        oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
        newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
        srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
        dstAccessMask = vk::AccessFlagBits2::eShaderRead;
        break;
      case TextureTransitionType::ShaderReadToRenderable:
        oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        newLayout = vk::ImageLayout::eColorAttachmentOptimal;
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

  ImTextureRef RendererBackend::getImGuiTextureHandle(const TexPtr& texture) {
    vkh::Texture& vkTexture =
        *std::dynamic_pointer_cast<vkh::Texture>(texture).get();

    return ImGui_ImplVulkan_AddTexture(
        *imGuiObjects->sampler,
        static_cast<VkImageView>(vkTexture.getImage().view),
        static_cast<VkImageLayout>(vk::ImageLayout::eShaderReadOnlyOptimal));
  }
} // namespace keptech::vkh
