#pragma once

#include "keptech/core/components/camera.hpp"
#include "keptech/core/components/transform.hpp"
#include "keptech/core/rendering/mesh.hpp"
#include "keptech/core/rendering/renderer.hpp"
#include <expected>
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

    std::expected<Mesh, std::string> loadMesh(const MeshData& data);
    std::expected<std::vector<Mesh>, std::string>
    loadMesh(std::string_view path);

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
    struct Buffers {
      size_t vertexEnd = 0;
      BufPtr vertex;
      size_t indexEnd = 0;
      BufPtr index;
    };

    Renderer(std::unique_ptr<IRendererBackend> backend, GBuffers gBuffers,
             Buffers&& buffers)
        : backend(std::move(backend)), gBuffers(std::move(gBuffers)),
          buffers(std::move(buffers)) {}

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

    Buffers buffers;
  };
} // namespace keptech
