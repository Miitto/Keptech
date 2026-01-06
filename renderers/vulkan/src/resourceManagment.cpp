#include "keptech/core/rendering/pipeline.hpp"
#include "keptech/vulkan/helpers/pipeline.hpp"
#include "keptech/vulkan/renderer.hpp"
#include "macros.hpp"
#include "vulkan/vulkan.hpp"

#include "vk-logger.hpp"
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/core/components/camera.hpp>
#include <keptech/core/renderer.hpp>
#include <keptech/core/rendering/gltf/loaded.hpp>
#include <keptech/core/window.hpp>
#include <vk_mem_alloc_structs.hpp>

namespace keptech::vkh {
  namespace {
    vk::ShaderStageFlagBits from(core::rendering::ShaderStages stages) {
      switch (stages) {
      case core::rendering::ShaderStages::Vertex:
        return vk::ShaderStageFlagBits::eVertex;
      case core::rendering::ShaderStages::Fragment:
        return vk::ShaderStageFlagBits::eFragment;
      case core::rendering::ShaderStages::Compute:
        return vk::ShaderStageFlagBits::eCompute;
      }
    }

    vk::ShaderStageFlags
    from(core::Bitflag<core::rendering::ShaderStages> stages) {
      using S = core::rendering::ShaderStages;
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
      meshHandles.emplace_back(std::move(meshHandle));
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
        vkcore.frameResources[nextFrameIndex].pools; // Use current frame pools

    auto res = vkh::Mesh::fromData(
        vkcore.device.logical, allocator,
        backgroundLoad ? vkcore.transferPool : *pools.graphics, meshData);

    if (!res) {
      return std::unexpected(res.error());
    }

    if (backgroundLoad)
      ongoingCommandBuffers.push_back(std::move(res.value().second));
    else {
      auto waitRes = vkcore.device.logical.waitForFences(
          *res.value().second.fence, VK_TRUE, UINT64_MAX);
      res.value().second.buffer.destroy(allocator);
      if (waitRes != vk::Result::eSuccess) {
        return std::unexpected("Failed to wait for mesh upload fence");
      }
    }

    auto handle = loadedMeshes.emplace(std::move(res.value().first));

    std::string name = meshData.name;

    core::rendering::Mesh::Handle meshHandle(
        core::SlotMapSmartHandle(handle, [this, name]() { unloadMesh(name); }));
    meshNameMap.emplace(meshData.name, meshHandle.handle.toWeak());

    return std::move(meshHandle);
  }

  void Renderer::unloadMesh(const std::string& name) {
    auto found = meshNameMap.find(name);
    if (found != meshNameMap.end()) {
      loadedMeshes.erase(found->second.get());
      meshNameMap.erase(found);
      VK_DEBUG("Unloaded mesh '{}'", name);
    }
  }

  std::optional<core::rendering::Mesh::Handle>
  Renderer::getMesh(const std::string& name) {
    auto found = meshNameMap.find(name);
    if (found != meshNameMap.end()) {
      core::SlotMapWeakHandle weakHandle = found->second;
      if (!weakHandle.valid()) {
        meshNameMap.erase(found);
        return std::nullopt;
      }
      auto handle = core::rendering::Mesh::Handle(core::SlotMapSmartHandle(
          weakHandle, [this, name]() { unloadMesh(name); }));
      return handle;
    }
    return std::nullopt;
  }

  std::expected<Renderer::MaterialHandle, std::string>
  Renderer::createMaterial(const Material::CreateInfo& createInfo) {
    GraphicsPipelineConfig config;

    std::vector<Shader> shaderModules;

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

    for (auto& shaderInfo : createInfo.pipelineConfig.shaders) {
      VKH_MAKE(shaderModule,
               Shader::create(vkcore.device.logical, shaderInfo.code,
                              shaderInfo.size),
               "Failed to create shader module");

      shaderModules.push_back(std::move(shaderModule));

      auto& shder = shaderModules.back();

      for (auto& stage : shaderInfo.stages) {
        vk::PipelineShaderStageCreateInfo stageInfo{
            .stage = from(stage.stage),
            .module = shder.get(),
            .pName = stage.name.data(),
        };

        shaderStages.push_back(stageInfo);
      }
    }

    config.shaders = shaderStages;

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

    config.layout.setLayouts.insert(config.layout.setLayouts.begin(),
                                    cameraObjects.layout);

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
    mat.stage = createInfo.stage;

    auto handle = loadedMaterials.emplace(std::move(mat));
    return MaterialHandle(core::SlotMapSmartHandle(handle, loadedMaterials));
  }

  std::expected<Shader, std::string>
  Renderer::createShader(const unsigned char* const code, size_t size) {
    return Shader::create(vkcore.device.logical, code, size);
  }

  std::expected<core::rendering::Texture::Handle, std::string>
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

    VMA_MAKE(
        vkImg,
        allocator.createImage(imageCreateInfo, allocCreateInfo, &allocInfo),
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
    core::SlotMapSmartHandle smHandle{handle, loadedTextures};

    return core::rendering::Texture::Handle(std::move(smHandle));
  }
} // namespace keptech::vkh
