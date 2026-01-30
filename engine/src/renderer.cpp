#include "keptech/renderer.hpp"
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

    deferredPipeline.reset();

    gBuffers.albedo.reset();
    gBuffers.normal.reset();
    gBuffers.depth.reset();

    buffers.cameraStaging.reset();
    buffers.vertex.reset();
    buffers.index.reset();
    buffers.instance.reset();

    backend.reset();

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

    GBuffers gBuffers;

    auto windowSize = window.getRenderSize();

    auto albedoRes = backend->createTexture(
        "gBuffer_albedo", glm::uvec3(windowSize.x, windowSize.y, 1),
        TextureFormat::RGBA8,
        TextureUsage::RenderTarget | TextureUsage::Sampled, 1);
    if (!albedoRes) {
      return std::unexpected(fmt::format("Failed to create albedo G-Buffer: {}",
                                         albedoRes.error()));
    }
    auto normalRes = backend->createTexture(
        "gBuffer_normal", glm::uvec3(windowSize.x, windowSize.y, 1),
        TextureFormat::RGBA8,
        TextureUsage::RenderTarget | TextureUsage::Sampled, 1);
    if (!normalRes) {
      return std::unexpected(fmt::format("Failed to create normal G-Buffer: {}",
                                         normalRes.error()));
    }
    auto depthRes = backend->createTexture(
        "gBuffer_depth", glm::uvec3(windowSize.x, windowSize.y, 1),
        TextureFormat::Depth16,
        TextureUsage::DepthStencil | TextureUsage::Sampled, 1);
    if (!depthRes) {
      return std::unexpected(
          fmt::format("Failed to create depth G-Buffer: {}", depthRes.error()));
    }

    gBuffers.albedo = std::move(albedoRes.value());
    gBuffers.normal = std::move(normalRes.value());
    gBuffers.depth = std::move(depthRes.value());

    auto vertexBufRes = backend->createBuffer(BufferCreateInfo{
        .name = "Vertex Buffer",
        .size = sizeof(Vertex) * 10'000,
        .usage = BufferUsage::Vertex | BufferUsage::TransferDst,
        .memoryType = BufferMemoryType::GpuOnly,
    });
    if (!vertexBufRes) {
      return std::unexpected(fmt::format("Failed to create vertex buffer: {}",
                                         vertexBufRes.error()));
    }
    auto indexBufRes = backend->createBuffer(BufferCreateInfo{
        .name = "Index Buffer",
        .size = sizeof(uint32_t) * 50'000,
        .usage = BufferUsage::Index | BufferUsage::TransferDst,
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
      return std::unexpected(fmt::format("Failed to create instance buffer: {}",
                                         instanceBufRes.error()));
    }

    auto deferredPipelineRes = backend->createPipeline({
        .shader = shaders::deferred,
        .layout = {.instanceDataTypes =
                       {
                           shaders::DataType::U32,
                           shaders::DataType::U32,
                       }},
    });
    if (!deferredPipelineRes) {
      return std::unexpected(
          fmt::format("Failed to create deferred pipeline: {}",
                      deferredPipelineRes.error()));
    }

    Renderer renderer{
        std::move(backend),
        std::move(gBuffers),
        std::move(deferredPipelineRes.value()),
        Buffers{
            .cameraStaging = std::move(cameraStagingRes.value()),
            .vertex = std::move(vertexBufRes.value()),
            .index = std::move(indexBufRes.value()),
            .instance = std::move(instanceBufRes.value()),
        },
    };

    renderer.initImGui();

    return std::move(renderer);
  }
} // namespace keptech
