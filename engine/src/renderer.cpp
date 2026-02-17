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
  void Renderer::newFrame() {
    backend->newFrame();

    ImGui_ImplSDL3_NewFrame();

    ImGui::NewFrame();
  }

  void Renderer::initImGui() {
    ImGui::CreateContext();
    backend->initImGui();
  }

  Renderer::~Renderer() {
    if (!backend) {
      return;
    }

    backend->preExit();

    backend->shutdownImGui();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }

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
      backend =
          std::make_unique<vkh::RendererBackend>(std::move(backendRes.value()));
    } break;
#endif
    }

    if (!backend) {
      return std::unexpected("Failed to create renderer backend");
    }

    {

      auto windowSize = window.getRenderSize();

      auto albedoRes = backend->createImage(
          {.name = "gBuffer_albedo",
           .size = glm::uvec3(windowSize.x, windowSize.y, 1),
           .format = TextureFormat::RGBA8,
           .usage = TextureUsage::RenderTarget | TextureUsage::Sampled,
           .mipLevels = 1});
      if (!albedoRes) {
        return std::unexpected(fmt::format(
            "Failed to create albedo G-Buffer: {}", albedoRes.error()));
      }
      auto normalRes = backend->createImage(
          {.name = "gBuffer_normal",
           .size = glm::uvec3(windowSize.x, windowSize.y, 1),
           .format = TextureFormat::RGBA8,
           .usage = TextureUsage::RenderTarget | TextureUsage::Sampled,
           .mipLevels = 1});
      if (!normalRes) {
        return std::unexpected(fmt::format(
            "Failed to create normal G-Buffer: {}", normalRes.error()));
      }
      auto emissiveAoRes = backend->createImage(
          {.name = "gBuffer_emissiveAo",
           .size = glm::uvec3(windowSize.x, windowSize.y, 1),
           .format = TextureFormat::RGBA8,
           .usage = TextureUsage::RenderTarget | TextureUsage::Sampled,
           .mipLevels = 1});
      auto metallicRoughnessRes = backend->createImage(
          {.name = "gBuffer_metallicRoughness",
           .size = glm::uvec3(windowSize.x, windowSize.y, 1),
           .format = TextureFormat::RG8,
           .usage = TextureUsage::RenderTarget | TextureUsage::Sampled,
           .mipLevels = 1});
      auto depthRes = backend->createImage(
          {.name = "gBuffer_depth",
           .size = glm::uvec3(windowSize.x, windowSize.y, 1),
           .format = TextureFormat::Depth16,
           .usage = TextureUsage::DepthStencil | TextureUsage::Sampled,
           .mipLevels = 1});
      if (!depthRes) {
        return std::unexpected(fmt::format(
            "Failed to create depth G-Buffer: {}", depthRes.error()));
      }

      auto diffuseRes = backend->createImage(
          {.name = "diffuseLightBuffer",
           .size = glm::uvec3(windowSize.x, windowSize.y, 1),
           .format = TextureFormat::RGBA16F,
           .usage = TextureUsage::RenderTarget | TextureUsage::Sampled,
           .mipLevels = 1});
      if (!diffuseRes) {
        return std::unexpected(fmt::format(
            "Failed to create diffuse light buffer: {}", diffuseRes.error()));
      }
      auto specularRes = backend->createImage(
          {.name = "specularLightBuffer",
           .size = glm::uvec3(windowSize.x, windowSize.y, 1),
           .format = TextureFormat::RGBA16F,
           .usage = TextureUsage::RenderTarget | TextureUsage::Sampled,
           .mipLevels = 1});
      if (!specularRes) {
        return std::unexpected(fmt::format(
            "Failed to create specular light buffer: {}", specularRes.error()));
      }

      auto combinedRes = backend->createImage(
          {.name = "combinedLightBuffer",
           .size = glm::uvec3(windowSize.x, windowSize.y, 1),
           .format = TextureFormat::RGBA16F,
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
          .memoryType = BufferMemoryType::GpuOnly,
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
          .memoryType = BufferMemoryType::GpuOnly,
      });
      if (!indexBufRes) {
        return std::unexpected(fmt::format("Failed to create index buffer: {}",
                                           indexBufRes.error()));
      }

      auto cameraStagingRes = backend->createBuffer(BufferCreateInfo{
          .name = "Camera Staging Buffer",
          .size = sizeof(components::Camera::Uniforms),
          .usage = BufferUsage::TransferSrc,
          .memoryType = BufferMemoryType::CpuToGpu,
      });
      if (!cameraStagingRes) {
        return std::unexpected(
            fmt::format("Failed to create camera staging buffer: {}",
                        cameraStagingRes.error()));
      }

      auto instanceBufRes = backend->createBuffer(BufferCreateInfo{
          .name = "Instance Buffer",
          .size = sizeof(InstanceData) * 10'000,
          .usage = BufferUsage::Uniform,
          .memoryType = BufferMemoryType::CpuToGpu,
      });
      if (!instanceBufRes) {
        return std::unexpected(fmt::format(
            "Failed to create instance buffer: {}", instanceBufRes.error()));
      }

      auto materialBufRes = backend->createBuffer(BufferCreateInfo{
          .name = "Material Buffer",
          .size = sizeof(DeferredMaterialData) * 100,
          .usage = BufferUsage::Uniform | BufferUsage::TransferDst |
                   BufferUsage::TransferSrc,
          .memoryType = BufferMemoryType::GpuOnly,
      });
      if (!materialBufRes) {
        return std::unexpected(fmt::format(
            "Failed to create material buffer: {}", materialBufRes.error()));
      }

      auto deferredPipelineRes = backend->createPipeline({
          .shader = shaders::deferred,
          .rasterizer =
              {
                  .cullMode = keptech::CullMode::Back,
                  .frontFace = keptech::FrontFace::Clockwise,
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
      });
      if (!deferredPipelineRes) {
        return std::unexpected(
            fmt::format("Failed to create deferred pipeline: {}",
                        deferredPipelineRes.error()));
      }
      auto pointLightPipelineRes = backend->createPipeline({
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
      });
      if (!pointLightPipelineRes) {
        return std::unexpected(
            fmt::format("Failed to create deferred point light pipeline: {}",
                        pointLightPipelineRes.error()));
      }

      auto lightCombinePipelineRes = backend->createPipeline({
          .shader = shaders::lightCombine,
          .attachments = {.colorFormats = {keptech::TextureFormat::RGBA16F}},
          .layout = {.pushConstantRanges = {PushConstantRange{
                         .offset = 0,
                         .size = sizeof(GBufferImageIndexData) +
                                 sizeof(LightBufferImageIndexData),
                         .stages = shaders::ShaderStages::Fragment,
                     }}},
      });
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

      {
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
            .memoryType = BufferMemoryType::CpuToGpu,
        });
        if (!materialStagingBufRes.has_value()) {
          return std::unexpected(
              fmt::format("Failed to create material staging buffer for "
                          "default material: {}",
                          materialStagingBufRes.error()));
        }

        memcpy(materialStagingBufRes.value()->getMapping(),
               &defaultMaterialData, sizeof(DeferredMaterialData));

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

      Renderer renderer{
          std::move(backend),
          {
              .albedo = std::move(albedoRes.value()),
              .normal = std::move(normalRes.value()),
              .emissiveAo = std::move(emissiveAoRes.value()),
              .metallicRoughness = std::move(metallicRoughnessRes.value()),
              .depth = std::move(depthRes.value()),
          },
          {
              .diffuse = std::move(diffuseRes.value()),
              .specular = std::move(specularRes.value()),
          },
          std::move(combinedRes.value()),
          Pipelines{
              .deferred = std::move(deferredPipelineRes.value()),
              .pointLight = std::move(pointLightPipelineRes.value()),
              .combineDeferred = std::move(lightCombinePipelineRes.value()),
          },
          Buffers{
              .cameraStaging = std::move(cameraStagingRes.value()),
              .vertex = std::move(vertexBufRes.value()),
              .index = std::move(indexBufRes.value()),
              .instance = std::move(instanceBufRes.value()),
              .material = std::move(materialBufRes.value()),
          },
          std::move(defaultMaterial),
      };

      renderer.initImGui();

      KT_INFO("Renderer created successfully");

      return std::move(renderer);
    }
  }
} // namespace keptech
