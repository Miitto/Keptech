#pragma once

#include "keptech/core/rendering/buffer.hpp"
#include "keptech/core/rendering/pipeline.hpp"
#include "keptech/core/rendering/texture.hpp"
#include <glm/glm.hpp>
#include <memory>

namespace keptech {
  enum class AttachmentLoadOp : uint8_t {
    Load = 0,
    Clear,
    DontCare,
  };

  enum class AttachmentStoreOp : uint8_t {
    Store = 0,
    DontCare,
  };

  struct RenderingColorAttachmentInfo {
    IImage* texture = nullptr;
    glm::vec4 clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    AttachmentLoadOp loadOp = AttachmentLoadOp::Load;
    AttachmentStoreOp storeOp = AttachmentStoreOp::Store;
  };

  struct RenderingDepthStencilAttachmentInfo {
    IImage* texture = nullptr;
    float clearDepth = 1.0f;
    uint32_t clearStencil = 0;
    AttachmentLoadOp loadOp = AttachmentLoadOp::Load;
    AttachmentStoreOp storeOp = AttachmentStoreOp::Store;
  };

  struct CommandBufferBeginRenderingInfo {
    glm::ivec2 renderAreaOffset{0, 0};
    glm::uvec2 renderAreaExtent{0, 0};
    std::vector<RenderingColorAttachmentInfo> colorAttachments{};
    RenderingDepthStencilAttachmentInfo depthAttachment{};
    RenderingDepthStencilAttachmentInfo stencilAttachment{};
  };

  enum class CmdBufType : uint8_t { Graphics = 0, Compute = 1, Transfer = 2 };

  class ICommandBuffer {
  public:
    virtual void begin() = 0;

    virtual void copyBufferToBuffer(IBuffer& src, IBuffer& dst, uint64_t size,
                                    uint64_t srcOffset = 0,
                                    uint64_t dstOffset = 0) = 0;

    virtual void
    beginRendering(const CommandBufferBeginRenderingInfo& info) = 0;
    virtual void setViewport(glm::vec2 offset, glm::vec2 extent) = 0;
    virtual void setScissor(glm::ivec2 offset, glm::uvec2 extent) = 0;

    virtual void bindPipeline(const IPipeline& pipeline) = 0;
    virtual void writePushConstants(const IPipeline& pipeline,
                                    Bitflag<shaders::ShaderStages> stages,
                                    uint32_t offset, uint32_t size,
                                    const void* data) = 0;

    virtual void bindIndexBuffer(IBuffer& buffer, uint64_t offset) = 0;
    virtual void bindVertexBuffer(uint32_t firstBinding,
                                  std::vector<IBuffer*> buffers,
                                  std::vector<uint64_t> offsets) = 0;

    virtual void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
                             uint32_t firstIndex = 0, int32_t vertexOffset = 0,
                             uint32_t firstInstance = 0) = 0;

    virtual void endRendering() = 0;
    virtual void end() = 0;

    [[nodiscard]] CmdBufType getType() const noexcept { return type; }

    ICommandBuffer(CmdBufType t) : type(t) {}

    ICommandBuffer() = default;
    ICommandBuffer(const ICommandBuffer&) = default;
    ICommandBuffer(ICommandBuffer&&) = default;
    ICommandBuffer& operator=(const ICommandBuffer&) = default;
    ICommandBuffer& operator=(ICommandBuffer&&) = default;
    virtual ~ICommandBuffer() = default;

  protected:
    CmdBufType type = CmdBufType::Graphics;
  };

  using CmdBufPtr = std::unique_ptr<ICommandBuffer>;
} // namespace keptech
