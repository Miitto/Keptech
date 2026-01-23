#include "keptech/core/components/renderObject.hpp"
#include "keptech/core/rendering/pipeline.hpp"
#include "keptech/vulkan/helpers/pipeline.hpp"
#include "keptech/vulkan/material.hpp"
#include "keptech/vulkan/renderer.hpp"
#include "macros.hpp"
#include "vulkan/vulkan.hpp"

#include "vk-logger.hpp"
#include <algorithm>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/core/components/camera.hpp>
#include <keptech/core/renderer.hpp>
#include <keptech/core/rendering/gltf/loaded.hpp>
#include <keptech/core/window.hpp>
#include <vk_mem_alloc_structs.hpp>

namespace keptech::vkh {

  std::expected<Renderer::InstanceBuffers, std::string>
  Renderer::InstanceBuffers::create(vma::Allocator& allocator,
                                    vk::raii::Device& device,
                                    size_t maxInstances) {
    VKH_MAKE(instanceStagingBuffer,
             AllocatedBuffer::create(
                 allocator,
                 vk::BufferCreateInfo{
                     .size = static_cast<vk::DeviceSize>(
                         sizeof(Renderer::InstanceData)),
                     .usage = vk::BufferUsageFlagBits::eTransferSrc,
                     .sharingMode = vk::SharingMode::eExclusive,
                 },
                 vma::AllocationCreateInfo{
                     .flags = vma::AllocationCreateFlagBits::eMapped,
                     .usage = vma::MemoryUsage::eCpuToGpu,
                 }),
             "Failed to create instance staging buffer.");

    VKH_MAKE(instanceDeviceBuffer,
             AddressedAllocatedBuffer::create(
                 device, allocator,
                 vk::BufferCreateInfo{
                     .size = static_cast<vk::DeviceSize>(
                         sizeof(Renderer::InstanceData)),
                     .usage = vk::BufferUsageFlagBits::eUniformBuffer |
                              vk::BufferUsageFlagBits::eTransferDst |
                              vk::BufferUsageFlagBits::eShaderDeviceAddress,
                     .sharingMode = vk::SharingMode::eExclusive,
                 },
                 vma::AllocationCreateInfo{
                     .usage = vma::MemoryUsage::eGpuOnly,
                 }),
             "Failed to create instance device buffer.");

    return Renderer::InstanceBuffers{
        .staging = instanceStagingBuffer,
        .device = instanceDeviceBuffer,
    };
  }

  void Renderer::InstanceBuffers::destroy(vma::Allocator& allocator) {
    staging.destroy(allocator);
    device.destroy(allocator);
  }

  std::expected<void, std::string>
  Renderer::InstanceBuffers::resize(vma::Allocator& allocator,
                                    vk::raii::Device& device,
                                    size_t newMaxInstances) {
    staging.destroy(allocator);
    this->device.destroy(allocator);

    VKH_MAKE(instanceStagingBuffer,
             AllocatedBuffer::create(
                 allocator,
                 vk::BufferCreateInfo{
                     .size = static_cast<vk::DeviceSize>(
                         sizeof(Renderer::InstanceData) * newMaxInstances),
                     .usage = vk::BufferUsageFlagBits::eTransferSrc,
                     .sharingMode = vk::SharingMode::eExclusive,
                 },
                 vma::AllocationCreateInfo{
                     .flags = vma::AllocationCreateFlagBits::eMapped,
                     .usage = vma::MemoryUsage::eCpuToGpu,
                 }),
             "Failed to create instance staging buffer.");
    staging = instanceStagingBuffer;

    VKH_MAKE(instanceDeviceBuffer,
             AddressedAllocatedBuffer::create(
                 device, allocator,
                 vk::BufferCreateInfo{
                     .size = static_cast<vk::DeviceSize>(
                         sizeof(Renderer::InstanceData) * newMaxInstances),
                     .usage = vk::BufferUsageFlagBits::eUniformBuffer |
                              vk::BufferUsageFlagBits::eTransferDst |
                              vk::BufferUsageFlagBits::eShaderDeviceAddress,
                     .sharingMode = vk::SharingMode::eExclusive,
                 },
                 vma::AllocationCreateInfo{
                     .usage = vma::MemoryUsage::eGpuOnly,
                 }),
             "Failed to create instance device buffer.");
    this->device = instanceDeviceBuffer;

    return {};
  }

  std::expected<void, std::string> Renderer::InstanceBuffers::copyToDevice(
      vk::raii::Device& device, const vk::raii::CommandBuffer& cmdBuffer,
      size_t instanceCount) {

    cmdBuffer.copyBuffer(
        staging.buffer, this->device.buffer,
        vk::BufferCopy{
            .size = static_cast<vk::DeviceSize>(instanceCount *
                                                sizeof(Renderer::InstanceData)),
        });

    return {};
  }

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

    vk::ShaderStageFlags from(core::Bitflag<shaders::ShaderStages> stages) {
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

    vk::Format from(core::rendering::Texture::Format format,
                    vk::Format defaultFormat) {
      using Format = core::rendering::Texture::Format;
      switch (format) {
      case Format::RGB8:
        return vk::Format::eR8G8B8Unorm;
      case Format::RGBA8:
        return vk::Format::eR8G8B8A8Unorm;
      case Format::Default:
        return defaultFormat;
      case core::rendering::Texture::Format::Undefined:
        return vk::Format::eUndefined;
      case core::rendering::Texture::Format::R8:
        return vk::Format::eR8Unorm;
      case core::rendering::Texture::Format::R16F:
        return vk::Format::eR16Sfloat;
      case core::rendering::Texture::Format::R32F:
        return vk::Format::eR32Sfloat;
      case core::rendering::Texture::Format::RG8:
        return vk::Format::eR8G8Unorm;
      case core::rendering::Texture::Format::RG16F:
        return vk::Format::eR16G16Sfloat;
      case core::rendering::Texture::Format::RG32F:
        return vk::Format::eR32G32Sfloat;
      case core::rendering::Texture::Format::RGB16F:
        return vk::Format::eR16G16B16Sfloat;
      case core::rendering::Texture::Format::RGB32F:
        return vk::Format::eR32G32B32Sfloat;
      case core::rendering::Texture::Format::RGBA16F:
        return vk::Format::eR16G16B16A16Sfloat;
      case core::rendering::Texture::Format::RGBA32F:
        return vk::Format::eR32G32B32A32Sfloat;
      case core::rendering::Texture::Format::Depth16:
        return vk::Format::eD16Unorm;
      case core::rendering::Texture::Format::Depth24Stencil8:
        return vk::Format::eD24UnormS8Uint;
      }
    }

    vk::ImageUsageFlags
    from(core::Bitflag<core::rendering::Texture::Usage> usage) {
      using Usage = core::rendering::Texture::Usage;

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

    vk::PrimitiveTopology from(core::rendering::Topology topology) {
      switch (topology) {
      case core::rendering::Topology::TriangleList:
        return vk::PrimitiveTopology::eTriangleList;
      case core::rendering::Topology::TriangleStrip:
        return vk::PrimitiveTopology::eTriangleStrip;
      case core::rendering::Topology::LineList:
        return vk::PrimitiveTopology::eLineList;
      case core::rendering::Topology::LineStrip:
        return vk::PrimitiveTopology::eLineStrip;
      case core::rendering::Topology::PointList:
        return vk::PrimitiveTopology::ePointList;
      default:
        return vk::PrimitiveTopology::eTriangleList;
      }
    }

    vk::PolygonMode from(core::rendering::PolygonMode mode) {
      switch (mode) {
      case core::rendering::PolygonMode::Fill:
        return vk::PolygonMode::eFill;
      case core::rendering::PolygonMode::Line:
        return vk::PolygonMode::eLine;
      case core::rendering::PolygonMode::Point:
        return vk::PolygonMode::ePoint;
      default:
        return vk::PolygonMode::eFill;
      }
    }

    vk::CullModeFlags from(core::rendering::CullMode mode) {
      switch (mode) {
      case core::rendering::CullMode::None:
        return vk::CullModeFlagBits::eNone;
      case core::rendering::CullMode::Front:
        return vk::CullModeFlagBits::eFront;
      case core::rendering::CullMode::Back:
        return vk::CullModeFlagBits::eBack;
      case core::rendering::CullMode::FrontAndBack:
        return vk::CullModeFlagBits::eFrontAndBack;
      default:
        return vk::CullModeFlagBits::eNone;
      }
    }

    vk::FrontFace from(core::rendering::FrontFace face) {
      switch (face) {
      case core::rendering::FrontFace::Clockwise:
        return vk::FrontFace::eClockwise;
      case core::rendering::FrontFace::CounterClockwise:
        return vk::FrontFace::eCounterClockwise;
      default:
        return vk::FrontFace::eClockwise;
      }
    }

    vk::BlendFactor from(core::rendering::BlendFactor factor) {
      switch (factor) {
      case core::rendering::BlendFactor::Zero:
        return vk::BlendFactor::eZero;
      case core::rendering::BlendFactor::One:
        return vk::BlendFactor::eOne;
      case core::rendering::BlendFactor::SrcAlpha:
        return vk::BlendFactor::eSrcAlpha;
      case core::rendering::BlendFactor::OneMinusSrcAlpha:
        return vk::BlendFactor::eOneMinusSrcAlpha;
      default:
        return vk::BlendFactor::eOne;
      }
    }

    vk::CompareOp from(core::rendering::DepthCompareOp op) {
      switch (op) {
      case core::rendering::DepthCompareOp::Never:
        return vk::CompareOp::eNever;
      case core::rendering::DepthCompareOp::Less:
        return vk::CompareOp::eLess;
      case core::rendering::DepthCompareOp::Equal:
        return vk::CompareOp::eEqual;
      case core::rendering::DepthCompareOp::LessEqual:
        return vk::CompareOp::eLessOrEqual;
      case core::rendering::DepthCompareOp::Greater:
        return vk::CompareOp::eGreater;
      case core::rendering::DepthCompareOp::NotEqual:
        return vk::CompareOp::eNotEqual;
      case core::rendering::DepthCompareOp::GreaterEqual:
        return vk::CompareOp::eGreaterOrEqual;
      case core::rendering::DepthCompareOp::Always:
        return vk::CompareOp::eAlways;
      default:
        return vk::CompareOp::eLess;
      }
    }
  } // namespace

  std::expected<std::vector<core::rendering::Mesh::Handle>, std::string>
  Renderer::loadMesh(const std::string_view path, bool backgroundLoad) {
    auto loadedGltfRes = core::gltf::LoadedGltf::fromFile(path);
    if (!loadedGltfRes) {
      return std::unexpected(fmt::format("Failed to load glTF file '{}': {}",
                                         path, loadedGltfRes.error()));
    }

    auto& loadedGltf = loadedGltfRes.value();

    if (loadedGltf.meshses.empty()) {
      return std::unexpected(
          fmt::format("No meshes found in glTF file '{}'", path));
    }

    std::vector<core::rendering::Mesh::Handle> meshHandles;
    for (auto& [name, meshDataPtr] : loadedGltf.meshses) {
      auto& meshData = *meshDataPtr;

      auto meshRes = meshFromData(meshData, backgroundLoad);

      if (!meshRes) {
        return std::unexpected(
            fmt::format("Failed to create mesh '{}' from glTF file '{}': {}",
                        name, path, meshRes.error()));
      }

      auto& meshHandle = meshRes.value();
      meshHandles.emplace_back(meshHandle);
      VK_DEBUG("Loaded mesh '{}' from glTF file '{}'", name, path);
    }
    VK_DEBUG("Loaded {} meshes from glTF file '{}'", meshHandles.size(), path);

    if (meshHandles.empty()) {
      return std::unexpected(
          fmt::format("No meshes created from glTF file '{}'", path));
    }

    return std::move(meshHandles);
  }

  std::expected<core::rendering::Mesh::Handle, std::string>
  Renderer::meshFromData(const core::rendering::MeshData& meshData,
                         bool backgroundLoad) {
    Pools& pools =
        vkcore.perFrame[thisFrameIndex].pools; // Use current frame pools

    auto res = vkh::Mesh::fromData(
        vkcore.device.logical, vkcore.allocator,
        backgroundLoad ? vkcore.transferPool : *pools.graphics, meshData);

    if (!res) {
      return std::unexpected(res.error());
    }

    if (backgroundLoad)
      ongoingCommandBuffers.push_back(std::move(res.value().second));
    else {
      auto waitRes = vkcore.device.logical.waitForFences(
          *res.value().second.fence, VK_TRUE, UINT64_MAX);
      res.value().second.buffer.destroy(vkcore.allocator);
      if (waitRes != vk::Result::eSuccess) {
        return std::unexpected("Failed to wait for mesh upload fence");
      }
    }

    auto handle = loadedMeshes.emplace(std::move(res.value().first));
    core::rendering::Mesh::Handle meshHandle{handle};

    VK_INFO("Created mesh '{}'", meshData.name);

    return meshHandle;
  }

  void Renderer::unloadMesh(const core::rendering::Mesh::Handle mesh) {
    loadedMeshes.erase(mesh);
  }

  vkh::Mesh* Renderer::getMeshData(const core::rendering::Mesh::Handle handle) {
    return loadedMeshes.get(handle);
  }

  std::expected<core::rendering::Material::Handle, std::string>
  Renderer::createMaterial(Material::CreateInfo createInfo) {
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
      createInfo.pipelineConfig.attachments =
          deferredPipelineAttachmentConfig();
      break;
    }
    case shaders::RenderingMode::Forward:
      // TODO: Implement forward / transparent attechment defaults
      break;
    case shaders::RenderingMode::Custom:
      break;
    }

    for (auto& colorFormat :
         createInfo.pipelineConfig.attachments.colorFormats) {
      config.rendering.colorAttachmentFormats.push_back(
          from(colorFormat, getSwapchainImageFormat()));
      config.blendAttachments.push_back(vk::PipelineColorBlendAttachmentState{
          .blendEnable = createInfo.pipelineConfig.blend.enableBlending,
          .srcColorBlendFactor = from(createInfo.pipelineConfig.blend.src),
          .dstColorBlendFactor = from(createInfo.pipelineConfig.blend.dst),
          .colorWriteMask =
              vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
      });
    }

    config.rendering.depthAttachmentFormat =
        from(createInfo.pipelineConfig.attachments.depthFormat,
             vk::Format::eD16Unorm);
    config.rendering.stencilAttachmentFormat =
        from(createInfo.pipelineConfig.attachments.stencilFormat,
             vk::Format::eUndefined);

    // Input Assembly
    config.inputAssembly.topology = from(createInfo.pipelineConfig.topology);

    // Rasterizer
    config.rasterizer.polygonMode =
        from(createInfo.pipelineConfig.rasterizer.polygonMode);
    config.rasterizer.cullMode =
        from(createInfo.pipelineConfig.rasterizer.cullMode);
    config.rasterizer.frontFace =
        from(createInfo.pipelineConfig.rasterizer.frontFace);

    config.depthStencilState.depthTestEnable =
        createInfo.pipelineConfig.depth.depthCompareOp.has_value();
    config.depthStencilState.depthWriteEnable =
        createInfo.pipelineConfig.depth.depthWrite;
    if (createInfo.pipelineConfig.depth.depthCompareOp.has_value()) {
      config.depthStencilState.depthCompareOp =
          from(createInfo.pipelineConfig.depth.depthCompareOp.value());
    }

    // Layout
    for (auto& pushConstant :
         createInfo.pipelineConfig.layout.pushConstantRanges) {
      vk::PushConstantRange range{
          .stageFlags = from(pushConstant.stages),
          .offset = pushConstant.offset,
          .size = pushConstant.size,
      };
      config.layout.pushConstantRanges.push_back(range);
    }

    vk::DeviceSize offset = 0;
    if (createInfo.pipelineConfig.layout.useVertexBuffer) {
      offset += sizeof(vk::DeviceAddress);
    }
    if (createInfo.pipelineConfig.layout.useModelMatrix) {
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

    Material mat{
        .pipeline = std::move(pipeline),
        .pipelineLayout = std::move(pipelineLayout),
    };

    mat.mode = createInfo.shader.mode;
    if (createInfo.shader.mode == shaders::RenderingMode::Deferred) {
      mat.stage = keptech::core::rendering::Material::Stage::Deferred;
    } else if (createInfo.shader.mode == shaders::RenderingMode::Forward) {
      if (createInfo.pipelineConfig.blend.enableBlending) {
        mat.stage = keptech::core::rendering::Material::Stage::Transparent;
      } else {
        mat.stage = keptech::core::rendering::Material::Stage::Opaque;
      }
    } else {
      return std::unexpected("Unsupported shader rendering mode");
    }

    mat.name = createInfo.shader.name;

    auto handle = loadedMaterials.emplace(std::move(mat));
    core::rendering::Material::Handle materialHandle{handle};

    VK_INFO("Created material '{}'", createInfo.shader.name);

    return materialHandle;
  }

  void Renderer::unloadMaterial(const core::rendering::Material::Handle name) {
    loadedMaterials.erase(name);
  }

  vkh::Material*
  Renderer::getMaterialData(const core::rendering::Material::Handle handle) {
    return loadedMaterials.get(handle);
  }

  std::expected<core::rendering::TextureHandle, std::string>
  Renderer::createTexture(glm::uvec3 size,
                          core::rendering::Texture::Format format,
                          core::Bitflag<core::rendering::Texture::Usage> usage,
                          uint32_t mipLevels, bool cpuAccess,
                          const void* data) {
    vk::ImageCreateInfo imageCreateInfo{
        .imageType = size.z == 1 ? vk::ImageType::e2D : vk::ImageType::e3D,
        .format = from(format, vk::Format::eR8G8B8A8Unorm),
        .extent = {.width = size.x, .height = size.y, .depth = size.z},
        .mipLevels = mipLevels,
        .arrayLayers = 1,
        .tiling =
            cpuAccess ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal,
        .usage = from(usage),
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined};
    vma::AllocationCreateInfo allocCreateInfo =
        cpuAccess
            ? vma::AllocationCreateInfo{.usage = vma::MemoryUsage::eCpuToGpu}
            : vma::AllocationCreateInfo{
                  .usage = vma::MemoryUsage::eGpuOnly,
                  .requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal};
    vma::AllocationInfo allocInfo = {};

    VMA_MAKE(vkImg,
             vkcore.allocator.createImage(imageCreateInfo, allocCreateInfo,
                                          &allocInfo),
             "Failed to allocate image.");

    auto imageViewCreateInfo = vk::ImageViewCreateInfo{
        .image = vkImg.first,
        .viewType =
            size.z == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::e3D,
        .format = imageCreateInfo.format,
        .subresourceRange =
            {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    VK_MAKE(imageView,
            vkcore.device.logical.createImageView(imageViewCreateInfo),
            "Failed to create image view.");

    AllocatedImage allocatedImage{
        .image = vkImg.first,
        .view = imageView,
        .alloc = vkImg.second,
        .extent = {.width = size.x, .height = size.y, .depth = size.z},
        .format = imageCreateInfo.format,
    };

    auto handle = loadedTextures.emplace(allocatedImage);

    return core::rendering::TextureHandle(handle);
  }
} // namespace keptech::vkh
