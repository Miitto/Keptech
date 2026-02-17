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

      auto deferredPipelineRes = backend->createPipeline({
          .shader = shaders::deferred,
          .rasterizer =
              {
                  .cullMode = keptech::CullMode::Back,
                  .frontFace = keptech::FrontFace::Clockwise,
              },
          .layout = {.pushConstantRanges = {PushConstantRange{
                         .offset = 0,
                         .size = sizeof(DeferredPushConstantData),
                         .stages = shaders::ShaderStages::Vertex,
                     }},
                     .instanceDataTypes =
                         {
                             shaders::DataType::F32_2,
                             shaders::DataType::F32_2,
                             shaders::DataType::F32,
                             shaders::DataType::U32,
                             shaders::DataType::F32_2,
                             shaders::DataType::F32_2,
                             shaders::DataType::F32,
                             shaders::DataType::U32,
                         }},
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

      Renderer renderer{
          std::move(backend),
          {
              .albedo = std::move(albedoRes.value()),
              .normal = std::move(normalRes.value()),
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
          },
      };

      renderer.initImGui();

      KT_INFO("Renderer created successfully");

      return std::move(renderer);
    }
  }
} // namespace keptech
