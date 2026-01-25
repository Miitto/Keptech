#include "keptech/renderer.hpp"
#include "keptech/core/window.hpp"

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

    gBuffers.albedo.reset();
    gBuffers.normal.reset();
    gBuffers.depth.reset();

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

    Renderer renderer{std::move(backend), std::move(gBuffers)};

    renderer.initImGui();

    return std::move(renderer);
  }
} // namespace keptech
