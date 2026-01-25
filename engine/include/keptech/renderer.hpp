#pragma once

#include "keptech/core/components/camera.hpp"
#include "keptech/core/components/transform.hpp"
#include "keptech/core/rendering/mesh.hpp"
#include "keptech/core/rendering/renderer.hpp"
#include <expected>
#include <future>
#include <memory>
#include <utility>

namespace keptech {
  class Scene;

  struct GBuffers {
    TexPtr albedo;
    TexPtr normal;
    TexPtr depth;
  };

  class Renderer {
    // End user use
  public:
    void setScene(Scene* newScene) { scene = newScene; }

    std::future<std::expected<MeshPtr, std::string>>
    loadMesh(const MeshData& data, bool backgroundLoad = false);
    std::future<std::expected<std::vector<MeshPtr>, std::string>>
    loadMesh(std::string_view path, bool backgroundLoad = false);

    inline std::expected<PipelinePtr, std::string>
    createPipeline(const PipelineCreateInfo& createInfo) {
      return backend->createPipeline(createInfo);
    }

    inline std::expected<TexPtr, std::string>
    createTexture(std::string name, glm::uvec3 size, TextureFormat format,
                  Bitflag<TextureUsage> usage, uint32_t mipLevels,
                  bool cpuAccess = false, const void* data = nullptr) {
      return backend->createTexture(std::move(name), size, format, usage,
                                    mipLevels, cpuAccess, data);
    }

    inline ImTextureRef getImGuiTextureHandle(const TexPtr& texture) {
      return backend->getImGuiTextureHandle(texture);
    }

    [[nodiscard]] const GBuffers& getGBuffers() const { return gBuffers; }

    // In Engine use
  public:
    static std::expected<Renderer, std::string>
    create(const RendererCreateInfo& createInfo,
           const core::window::Window& window);

    void newFrame();
    void render();

    Renderer(const Renderer&) = delete;
    Renderer(Renderer&&) = default;
    Renderer& operator=(const Renderer&) = delete;
    Renderer& operator=(Renderer&&) = default;
    ~Renderer();

  private:
    Renderer(std::unique_ptr<IRendererBackend> backend, GBuffers gBuffers)
        : backend(std::move(backend)), gBuffers(std::move(gBuffers)) {}

    void initImGui();

    struct CameraData {
      components::Transform* transform = nullptr;
      components::Camera* camera = nullptr;
    };

    struct FrameData {
      CmdBufPtr graphicsCmdBuf;
      CameraData cameraData;
    };

    void drawDeferredPass(const FrameData&);
    void drawLightingPass(const FrameData&);
    void combineDeferredPass(const FrameData&);
    void drawForwardPass(const FrameData&);
    void drawTransparentPass(const FrameData&);
    void drawUIPass(const FrameData&);

    Scene* scene = nullptr;

    std::unique_ptr<IRendererBackend> backend;
    GBuffers gBuffers;
  };
} // namespace keptech
