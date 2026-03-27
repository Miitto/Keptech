#include "keptech/renderer.hpp"
#include "keptech/core/kt-logger.hpp"
#include "keptech/core/rendering/pipeline.hpp"
#include "keptech/core/window.hpp"
#include <keptech/core/rendering/buffer.hpp>

#include "shaders.inl"
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/imgui.h>

#ifdef KEPTECH_RENDERER_VULKAN
#include <keptech/vulkan/renderer.hpp>
#endif

namespace keptech {
  std::expected<Renderer, std::string>
  Renderer::create(const RendererCreateInfo& createInfo,
                   const core::window::Window& window) {
    std::unique_ptr<IRendererBackend> backend;

    switch (createInfo.backendType) {
#ifdef KEPTECH_RENDERER_VULKAN
    case RendererBackendType::Vulkan: {
      auto backendRes = vkh::RendererBackend::create(createInfo, window);
      if (!backendRes) {
        return std::unexpected(backendRes.error());
      }
      backend = std::make_unique<vkh::Renderer>(std::move(backendRes.value()));
    } break;
#endif
    }

    if (!backend) {
      return std::unexpected("Failed to create renderer backend");
    }

    for (auto format : normalFormats) {
      if (backend->canRenderToFormat(format)) {
        formats.normal = format;
        break;
      }
    }

    for (auto format : metallicRoughnessFormats) {
      if (backend->canRenderToFormat(format)) {
        formats.metallicRoughness = format;
        break;
      }
    }
    for (auto depthFormat : depthFormats) {
      if (backend->canRenderToFormat(depthFormat)) {
        formats.depth = depthFormat;
        break;
      }
    }

    for (auto format : lightFormats) {
      if (backend->canRenderToFormat(format)) {
        formats.diffuse = format;
        formats.specular = format;
        formats.combined = format;
        break;
      }
    }
    if (formats.normal == TextureFormat::Undefined) {
      return std::unexpected("No supported normal format found for G-Buffer");
    }
    if (formats.metallicRoughness == TextureFormat::Undefined) {
      return std::unexpected(
          "No supported metallic-roughness format found for G-Buffer");
    }
    if (formats.depth == TextureFormat::Undefined) {
      return std::unexpected("No supported depth format found for G-Buffer");
    }

    if (formats.diffuse == TextureFormat::Undefined) {
      return std::unexpected(
          "No supported diffuse light format found for lighting buffer");
    }
    if (formats.specular == TextureFormat::Undefined) {
      return std::unexpected(
          "No supported specular light format found for lighting buffer");
    }
    if (formats.combined == TextureFormat::Undefined) {
      return std::unexpected(
          "No supported combined light format found for lighting buffer");
    }

    auto windowSize = window.getRenderSize();

    auto albedoRes = backend->createImage(
        {.name = "gBuffer_albedo",
         .size = glm::uvec3(windowSize.x, windowSize.y, 1),
         .format = formats.albedo,
         .usage = TextureUsage::RenderTarget | TextureUsage::Sampled,
         .mipLevels = 1});
    if (!albedoRes) {
      return std::unexpected(fmt::format("Failed to create albedo G-Buffer: {}",
                                         albedoRes.error()));
    }
    auto normalRes = backend->createImage(
        {.name = "gBuffer_normal",
         .size = glm::uvec3(windowSize.x, windowSize.y, 1),
         .format = formats.normal,
         .usage = TextureUsage::RenderTarget | TextureUsage::Sampled,
         .mipLevels = 1});
    if (!normalRes) {
      return std::unexpected(fmt::format("Failed to create normal G-Buffer: {}",
                                         normalRes.error()));
    }
    auto emissiveAoRes = backend->createImage(
        {.name = "gBuffer_emissiveAo",
         .size = glm::uvec3(windowSize.x, windowSize.y, 1),
         .format = formats.emissiveAo,
         .usage = TextureUsage::RenderTarget | TextureUsage::Sampled,
         .mipLevels = 1});
    auto metallicRoughnessRes = backend->createImage(
        {.name = "gBuffer_metallicRoughness",
         .size = glm::uvec3(windowSize.x, windowSize.y, 1),
         .format = formats.metallicRoughness,
         .usage = TextureUsage::RenderTarget | TextureUsage::Sampled,
         .mipLevels = 1});
    auto depthRes = backend->createImage(
        {.name = "gBuffer_depth",
         .size = glm::uvec3(windowSize.x, windowSize.y, 1),
         .format = formats.depth,
         .usage = TextureUsage::DepthStencil | TextureUsage::Sampled,
         .mipLevels = 1});
    if (!depthRes) {
      return std::unexpected(
          fmt::format("Failed to create depth G-Buffer: {}", depthRes.error()));
    }

    auto diffuseRes = backend->createImage(
        {.name = "diffuseLightBuffer",
         .size = glm::uvec3(windowSize.x, windowSize.y, 1),
         .format = formats.diffuse,
         .usage = TextureUsage::RenderTarget | TextureUsage::Sampled,
         .mipLevels = 1});
    if (!diffuseRes) {
      return std::unexpected(fmt::format(
          "Failed to create diffuse light buffer: {}", diffuseRes.error()));
    }
    auto specularRes = backend->createImage(
        {.name = "specularLightBuffer",
         .size = glm::uvec3(windowSize.x, windowSize.y, 1),
         .format = formats.specular,
         .usage = TextureUsage::RenderTarget | TextureUsage::Sampled,
         .mipLevels = 1});
    if (!specularRes) {
      return std::unexpected(fmt::format(
          "Failed to create specular light buffer: {}", specularRes.error()));
    }

    auto combinedRes = backend->createImage(
        {.name = "combinedLightBuffer",
         .size = glm::uvec3(windowSize.x, windowSize.y, 1),
         .format = formats.combined,
         .usage = TextureUsage::RenderTarget | TextureUsage::Sampled,
         .mipLevels = 1});
    if (!combinedRes) {
      return std::unexpected(fmt::format(
          "Failed to create combined light buffer: {}", combinedRes.error()));
    }

    auto vertexBufRes = backend->createBuffer({
        .name = "Vertex Buffer",
        .size = sizeof(Vertex) * 10'000,
        .usage = BufferUsage::Vertex | BufferUsage::TransferDst |
                 BufferUsage::TransferSrc,
        .memoryType = BufferMemoryType::PreferDevice,
        .map = BufferMapType::AllowTransferInstead | BufferMapType::SeqWrite,
    });
    if (!vertexBufRes) {
      return std::unexpected(fmt::format("Failed to create vertex buffer: {}",
                                         vertexBufRes.error()));
    }
    auto indexBufRes = backend->createBuffer(BufferCreateInfo{
        .name = "Index Buffer",
        .size = sizeof(uint32_t) * 50'000,
        .usage = BufferUsage::Index | BufferUsage::TransferDst |
                 BufferUsage::TransferSrc,
        .memoryType = BufferMemoryType::PreferDevice,
        .map = BufferMapType::AllowTransferInstead | BufferMapType::SeqWrite,

    });
    if (!indexBufRes) {
      return std::unexpected(fmt::format("Failed to create index buffer: {}",
                                         indexBufRes.error()));
    }

    auto instanceBufRes = backend->createBuffer(BufferCreateInfo{
        .name = "Instance Buffer",
        .size = sizeof(InstanceData) * 10'000,
        .usage = BufferUsage::Uniform,
        .memoryType = BufferMemoryType::PreferDevice,
        .map = BufferMapType::AllowTransferInstead | BufferMapType::SeqWrite,

    });
    if (!instanceBufRes) {
      return std::unexpected(fmt::format("Failed to create instance buffer: {}",
                                         instanceBufRes.error()));
    }

    auto materialBufRes = backend->createBuffer(BufferCreateInfo{
        .name = "Material Buffer",
        .size = sizeof(DeferredMaterialData) * 100,
        .usage = BufferUsage::Uniform | BufferUsage::TransferDst |
                 BufferUsage::TransferSrc,
        .memoryType = BufferMemoryType::PreferDevice,
        .map = BufferMapType::AllowTransferInstead | BufferMapType::SeqWrite,
    });
    if (!materialBufRes) {
      return std::unexpected(fmt::format("Failed to create material buffer: {}",
                                         materialBufRes.error()));
    }
    PipelineCreateInfo deferredPipelineInfo{
        .shader = shaders::deferred,
        .rasterizer =
            {
                .cullMode = keptech::CullMode::Back,
                .frontFace = keptech::FrontFace::CounterClockwise,
            },
        .layout =
            {
                .pushConstantRanges = {PushConstantRange{
                    .offset = 0,
                    .size = sizeof(DeferredPushConstantData),
                    .stages = shaders::ShaderStages::Vertex |
                              shaders::ShaderStages::Fragment,
                }},
                .instanceDataTypes =
                    {
                        shaders::DataType::F32_4, // Albedo color
                        shaders::DataType::F32_3, // Emissive color
                        shaders::DataType::F32,   // Metallic
                        shaders::DataType::F32_2, // Albedo UV scale
                        shaders::DataType::F32_2, // Albedo UV offset
                        shaders::DataType::F32,   // Albedo UV rotation
                        shaders::DataType::U32,   // Albedo texture index
                        shaders::DataType::F32_2, // Bump UV scale
                        shaders::DataType::F32_2, // Bump UV offset
                        shaders::DataType::F32,   // Bump UV rotation
                        shaders::DataType::U32,   // Bump texture index
                        shaders::DataType::F32_2, // Emissive UV scale
                        shaders::DataType::F32_2, // Emissive UV offset
                        shaders::DataType::F32,   // Emissive UV rotation
                        shaders::DataType::U32,   // Emissive texture index
                        shaders::DataType::F32_2, // MetRough UV scale
                        shaders::DataType::F32_2, // MetRough UV offset
                        shaders::DataType::F32,   // MetRough UV rotation
                        shaders::DataType::U32,   // MetRough texture index
                        shaders::DataType::F32_2, // AO UV scale
                        shaders::DataType::F32_2, // AO UV offset
                        shaders::DataType::F32,   // AO UV rotation
                        shaders::DataType::U32,   // AO texture index
                        shaders::DataType::F32,   // Roughness
                    },
            },
    };
    preprocessPipelineCreateInfo(deferredPipelineInfo, formats, *backend);
    auto deferredPipelineRes =
        backend->createPipeline(std::move(deferredPipelineInfo));
    if (!deferredPipelineRes) {
      return std::unexpected(
          fmt::format("Failed to create deferred pipeline: {}",
                      deferredPipelineRes.error()));
    }
    PipelineCreateInfo pointLightPipelineInfo{
        .shader = shaders::pointLight,
        .rasterizer =
            {
                .cullMode = keptech::CullMode::Back,
                .frontFace = keptech::FrontFace::Clockwise,
            },
        .blend =
            {
                .enableBlending = true,
                .src = BlendFactor::One,
                .dst = BlendFactor::One,
            },
        .layout = {.pushConstantRanges = {PushConstantRange{
                       .offset = 0,
                       .size = sizeof(PointLightPushConstantData) +
                               sizeof(GBufferImageIndexData),
                       .stages = shaders::ShaderStages::Vertex |
                                 shaders::ShaderStages::Fragment,
                   }}},
    };
    preprocessPipelineCreateInfo(pointLightPipelineInfo, formats, *backend);
    auto pointLightPipelineRes =
        backend->createPipeline(std::move(pointLightPipelineInfo));
    if (!pointLightPipelineRes) {
      return std::unexpected(
          fmt::format("Failed to create deferred point light pipeline: {}",
                      pointLightPipelineRes.error()));
    }

    PipelineCreateInfo lightCombinePipelineInfo{
        .shader = shaders::lightCombine,
        .attachments = {.colorFormats = {formats.combined}},
        .layout = {.pushConstantRanges = {PushConstantRange{
                       .offset = 0,
                       .size = sizeof(GBufferImageIndexData) +
                               sizeof(LightBufferImageIndexData),
                       .stages = shaders::ShaderStages::Fragment,
                   }}},
    };
    preprocessPipelineCreateInfo(lightCombinePipelineInfo, formats, *backend);
    auto lightCombinePipelineRes =
        backend->createPipeline(std::move(lightCombinePipelineInfo));
    if (!lightCombinePipelineRes) {
      return std::unexpected(
          fmt::format("Failed to create light combine pipeline: {}",
                      lightCombinePipelineRes.error()));
    }

    DeferredMaterialData defaultMaterialData{
        .albedoColor = glm::vec4{1.f},
        .emissiveColor = glm::vec3{0.f},
        .metallic = 0.f,
        .albedo = {},
        .bump = {},
        .emissive = {},
        .metallicRoughness = {},
        .ao = {},
        .roughness = 1.f,
    };

    uint8_t* materialMapping =
        static_cast<uint8_t*>(materialBufRes.value()->getMapping());
    if (materialMapping != nullptr) {
      memcpy(materialMapping, &defaultMaterialData,
             sizeof(DeferredMaterialData));
    } else {
      auto cmdBufRes = backend->createCmdBuffer(CmdBufType::Compute);
      if (!cmdBufRes) {
        return std::unexpected(fmt::format(
            "Failed to create command buffer for default material upload: {}",
            cmdBufRes.error()));
      }

      cmdBufRes.value()->begin();

      auto materialStagingBufRes = backend->createBuffer(BufferCreateInfo{
          .name = "Material Staging Buffer",
          .size = sizeof(DeferredMaterialData),
          .usage = BufferUsage::TransferSrc,
          .memoryType = BufferMemoryType::Auto,
          .map = BufferMapType::SeqWrite,
      });
      if (!materialStagingBufRes.has_value()) {
        return std::unexpected(
            fmt::format("Failed to create material staging buffer for "
                        "default material: {}",
                        materialStagingBufRes.error()));
      }

      memcpy(materialStagingBufRes.value()->getMapping(), &defaultMaterialData,
             sizeof(DeferredMaterialData));

      cmdBufRes.value()->copyBufferToBuffer(
          *materialStagingBufRes.value().get(), *materialBufRes.value().get(),
          sizeof(DeferredMaterialData), 0, 0);

      cmdBufRes.value()->end();

      std::vector<IRendererBackend::SubmitInfo> submitInfos;
      submitInfos.push_back({
          .commandBuffer = std::move(cmdBufRes.value()),
          .trackedBuffers = {materialStagingBufRes.value(),
                             materialBufRes.value()},
      });
      backend->submitCommandBuffers({std::move(submitInfos)});
    }

    MaterialPtr defaultMaterial = std::make_shared<Material>(
        deferredPipelineRes.value(),
        std::vector<keptech::MaterialData>{
            glm::vec4{}, glm::vec3{}, 0.f,      glm::vec2{1.f, 1.f},
            glm::vec2{}, 0.f,         ImgPtr(), glm::vec2{1.f, 1.f},
            glm::vec2{}, 0.f,         ImgPtr(), glm::vec2{1.f, 1.f},
            glm::vec2{}, 0.f,         ImgPtr(), glm::vec2{1.f, 1.f},
            glm::vec2{}, 0.f,         ImgPtr(), glm::vec2{1.f, 1.f},
            glm::vec2{}, 0.f,         ImgPtr(), 1.f},
        0);

    Members m{
        .backend = std::move(backend),
        .gBuffers =
            {
                .albedo = std::move(albedoRes.value()),
                .normal = std::move(normalRes.value()),
                .emissiveAo = std::move(emissiveAoRes.value()),
                .metallicRoughness = std::move(metallicRoughnessRes.value()),
                .depth = std::move(depthRes.value()),
            },
        .lightingBuffers =
            {
                .diffuse = std::move(diffuseRes.value()),
                .specular = std::move(specularRes.value()),
            },
        .lightCombinedBuffer = std::move(combinedRes.value()),
        .pipelines =
            Pipelines{
                .deferred = std::move(deferredPipelineRes.value()),
                .pointLight = std::move(pointLightPipelineRes.value()),
                .combineDeferred = std::move(lightCombinePipelineRes.value()),
            },
        .buffers =
            Buffers{
                .vertex = std::move(vertexBufRes.value()),
                .index = std::move(indexBufRes.value()),
                .instance = std::move(instanceBufRes.value()),
                .material = std::move(materialBufRes.value()),
            },
        .defaultMaterial = std::move(defaultMaterial),
    };

    Renderer renderer(std::move(m));

    renderer.initImGui();

    KT_INFO("Renderer created successfully");

    return std::move(renderer);
  }
} // namespace keptech
