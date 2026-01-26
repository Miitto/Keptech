#pragma once

#include "glm/fwd.hpp"
#include "keptech/core/rendering/buffer.hpp"
#include "keptech/core/rendering/commandBuffer.hpp"
#include "keptech/core/rendering/pipeline.hpp"
#include "keptech/core/rendering/texture.hpp"
#include <concepts>
#include <expected>
#include <string>

namespace keptech::core {
  class Scene;

  namespace window {
    class Window;
  }
} // namespace keptech::core

namespace keptech {

  enum class RendererBackendType : uint8_t { Vulkan = 0 };

  struct RendererCreateInfo {
    const char* applicationName = "Keptech App";
    RendererBackendType backendType = RendererBackendType::Vulkan;
  };

  class IRendererBackend {
  public:
    virtual std::expected<BufPtr, std::string>
    createBuffer(const BufferCreateInfo&) = 0;

    virtual std::expected<TexPtr, std::string>
    createTexture(std::string name, glm::uvec3 size, TextureFormat format,
                  Bitflag<TextureUsage> usage, uint32_t mipLevels,
                  bool cpuAccess = false, const void* data = nullptr) = 0;

    virtual ImTextureRef getImGuiTextureHandle(const TexPtr& texture) = 0;

    virtual std::expected<PipelinePtr, std::string>
    createPipeline(PipelineCreateInfo createInfo) = 0;

    virtual std::expected<CmdBufPtr, std::string>
        createCmdBuffer(CmdBufType) = 0;

    virtual void
    textureLayoutTransition(const CmdBufPtr&,
                            const std::vector<TextureTransition>&) = 0;

    virtual void newFrame() = 0;

    /// Do work necessary at the start of the frame, such as transitioning the
    /// swapchain image. Creates the main graphics command buffer for the frame.
    virtual std::expected<CmdBufPtr, std::string> startFrame() = 0;

    virtual void writeCameraMatrices(const CmdBufPtr&,
                                     const BufPtr& stagingBuffer) = 0;
    virtual void bindGlobalDescriptorSets(const CmdBufPtr&, const IPipeline&,
                                          Bitflag<shaders::ShaderStages>) = 0;

    virtual void renderImGui(const CmdBufPtr&) = 0;

    struct SubmitInfo {
      CmdBufPtr commandBuffer;
      std::vector<BufPtr> trackedBuffers;
    };
    virtual void submitCommandBuffers(std::vector<SubmitInfo>) = 0;
    virtual void endFrame(CmdBufPtr&&) = 0;

    virtual void initImGui() = 0;

    virtual void preExit() = 0;

    IRendererBackend() = default;
    IRendererBackend(const IRendererBackend&) = default;
    IRendererBackend(IRendererBackend&&) = default;
    IRendererBackend& operator=(const IRendererBackend&) = default;
    IRendererBackend& operator=(IRendererBackend&&) = default;
    virtual ~IRendererBackend() = default;
  };

  template <typename T>
  concept CRenderer =
      requires(T a, const RendererCreateInfo& ci, const core::window::Window& w,
               core::Scene& scene) {
        { T::create(ci, w) } -> std::same_as<std::expected<T, std::string>>;
        { a.newFrame() } -> std::same_as<void>;
        { a.setScene(scene) } -> std::same_as<void>;
        { a.render() } -> std::same_as<void>;
        { T::getName() } -> std::same_as<const char*>;
      };

  [[nodiscard]] inline AttachmentConfig deferredPipelineAttachmentConfig() {
    using ac = AttachmentConfig;
    using F = TextureFormat;
    return ac{.colorFormats = {F::RGBA8, F::RGBA8}, .depthFormat = F::Depth16};
  }
} // namespace keptech
