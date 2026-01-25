#pragma once

#include "keptech/core/rendering/mesh.hpp"
#include "keptech/core/rendering/renderer.hpp"
#include <expected>
#include <future>
#include <memory>

namespace keptech {

  struct GBuffers {
    std::unique_ptr<ITexture> albedo;
    std::unique_ptr<ITexture> normal;
    std::unique_ptr<ITexture> depth;
  };

  class Renderer {
    // End user use
  public:
    std::future<std::expected<std::vector<UMeshPtr>, std::string>>
    loadMesh(const MeshData& data, bool backgroundLoad = false);

    std::expected<UPipelinePtr, std::string>
    createPipeline(const PipelineCreateInfo& createInfo) {
      return backend->createPipeline(createInfo);
    }

    std::expected<UTexPtr, std::string>
    createTexture(glm::uvec3 size, TextureFormat format,
                  Bitflag<TextureUsage> usage, uint32_t mipLevels,
                  bool cpuAccess = false, const void* data = nullptr) {
      return backend->createTexture(size, format, usage, mipLevels, cpuAccess,
                                    data);
    }

    // In Engine use
  public:
    static std::expected<Renderer, std::string>
    create(const RendererCreateInfo& createInfo,
           const core::window::Window& window);

    void newFrame();
    void render();

    ~Renderer();

  private:
    void initImGui();

    Scene* scene = nullptr;

    std::unique_ptr<IRendererBackend> backend;
    GBuffers gBuffers;
  };
} // namespace keptech
