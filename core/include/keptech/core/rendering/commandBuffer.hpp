#pragma once

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
    ITexture* texture = nullptr;
    glm::vec4 clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    AttachmentLoadOp loadOp = AttachmentLoadOp::Load;
    AttachmentStoreOp storeOp = AttachmentStoreOp::Store;
  };

  struct RenderingDepthStencilAttachmentInfo {
    ITexture* texture = nullptr;
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

  class ICommandBuffer {
  public:
    virtual void begin() = 0;
    virtual void
    beginRenderering(const CommandBufferBeginRenderingInfo& info) = 0;
    virtual void endRendering() = 0;
    virtual void end() = 0;

    ICommandBuffer() = default;
    ICommandBuffer(const ICommandBuffer&) = default;
    ICommandBuffer(ICommandBuffer&&) = default;
    ICommandBuffer& operator=(const ICommandBuffer&) = default;
    ICommandBuffer& operator=(ICommandBuffer&&) = default;
    virtual ~ICommandBuffer() = default;
  };

  using UCmdBufPtr = std::unique_ptr<ICommandBuffer>;
  using SCmdBufPtr = std::shared_ptr<ICommandBuffer>;
} // namespace keptech
