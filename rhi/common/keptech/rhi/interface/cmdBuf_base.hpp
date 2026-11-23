#pragma once

#include "keptech/rhi/bufferRef.hpp"
#include "keptech/rhi/imageLayout.hpp"
#include "keptech/rhi/imageRef.hpp"
#include "keptech/rhi/loadStoreOps.hpp"
#include <span>

namespace kt {
  namespace maths {
    struct Viewport;
    template <typename T, typename U> struct Rect2D;
  } // namespace maths
} // namespace kt

namespace kt::rhi {
  class CommandBuffer;
  struct Pipeline;
  class DescriptorSet;

  class CommandBufferBase {
  public:
    friend class CommandBuffer;
    CommandBuffer& bindGraphicsPipeline(this CommandBuffer& self, const Pipeline& pipeline);
    CommandBuffer& bindComputePipeline(this CommandBuffer& self, const Pipeline& pipeline);

    CommandBuffer& clearColorImage(this CommandBuffer& self, const rhi::ImageRef& image, const std::array<float, 4>& clearColor);
    CommandBuffer& clearDepthStencilImage(this CommandBuffer& self, const rhi::ImageRef& image, float clearDepth, uint8_t clearStencil);

    CommandBuffer& setViewport(this CommandBuffer& self, const maths::Viewport& viewport);
    CommandBuffer& setScissor(this CommandBuffer& self, const maths::Rect2D<uint32_t, uint32_t>& rect);

    struct ColorAttachmentDesc {
      ImageRef imageRef{};
      LoadOp loadOp = LoadOp::DontCare;
      StoreOp storeOp = StoreOp::Store;
      uint32_t mipLevel = 0;
      std::array<float, 4> clearColor{};
    };
    struct DepthStencilAttachmentDesc {
      ImageRef imageRef{};
      LoadOp loadOp = LoadOp::DontCare;
      StoreOp storeOp = StoreOp::Store;
      float clearDepth = 1.0f;
      uint8_t clearStencil = 0;
    };
    CommandBuffer& beginRendering(this CommandBuffer& self, const std::span<ColorAttachmentDesc> colorAttachments,
                                  std::optional<DepthStencilAttachmentDesc> depthStencilAttachment = std::nullopt);

    CommandBuffer& bindVertexBuffer(this CommandBuffer& self, size_t slot, const BufferRef& buffer, size_t stride, size_t offset = 0);
    template <typename T, typename Self>
    CommandBuffer& bindVertexBuffer(this Self& self, size_t slot, const BufferRef& buffer, size_t offset = 0) {
      return self.bindVertexBuffer(slot, buffer, sizeof(T), offset);
    }

    struct VertexBufferBinding {
      BufferRef buffer;
      size_t stride = 0;
      size_t offset = 0;
    };
    CommandBuffer& bindVertexBuffers(this CommandBuffer& self, size_t firstSlot, const std::span<const VertexBufferBinding> bindings);

    enum class IndexType : uint8_t { UInt16, UInt32 };
    CommandBuffer& bindIndexBuffer(this CommandBuffer& self, const BufferRef& buffer, IndexType indexType = IndexType::UInt32,
                                   size_t offset = 0);

    CommandBuffer& writeGraphicsPushConstants(this CommandBuffer& self, const void* data, size_t size, size_t offset = 0);
    template <typename T, typename Self> CommandBuffer& writeGraphicsPushConstants(this Self& self, const T& data, size_t offset = 0) {
      return self.writeGraphicsPushConstants(&data, sizeof(T), offset);
    }
    CommandBuffer& writeComputePushConstants(this CommandBuffer& self, const void* data, size_t size, size_t offset = 0);
    template <typename T, typename Self> CommandBuffer& writeComputePushConstants(this Self& self, const T& data, size_t offset = 0) {
      return self.writeComputePushConstants(&data, sizeof(T), offset);
    }

    CommandBuffer& pushGraphicsUniformBuffer(this CommandBuffer& self, const BufferRef& buffer, uint32_t binding, size_t offset = 0);
    CommandBuffer& pushGraphicsStorageBuffer(this CommandBuffer& self, const BufferRef& buffer, uint32_t binding, size_t offset = 0);

    CommandBuffer& bindGraphicsDescriptorSet(this CommandBuffer& self, const DescriptorSet& set, uint32_t setIndex = 0);

    CommandBuffer& draw(this CommandBuffer& self, uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0,
                        uint32_t firstInstance = 0);
    CommandBuffer& drawIndexed(this CommandBuffer& self, uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0,
                               int32_t vertexOffset = 0, uint32_t firstInstance = 0);

    CommandBuffer& drawIndirect(this CommandBuffer& self, const BufferRef& buffer, uint32_t drawCount, uint32_t offset = 0);
    CommandBuffer& drawIndexedIndirect(this CommandBuffer& self, const BufferRef& buffer, uint32_t drawCount, uint32_t offset = 0);

    CommandBuffer& drawIndirectCount(this CommandBuffer& self, const BufferRef& buffer, const BufferRef& countBuffer, uint32_t maxDrawCount,
                                     uint32_t drawOffset = 0, uint32_t countBufferOffset = 0);
    CommandBuffer& drawIndexedIndirectCount(this CommandBuffer& self, const BufferRef& buffer, const BufferRef& countBuffer,
                                            uint32_t maxDrawCount, uint32_t drawOffset = 0, uint32_t countBufferOffset = 0);

    CommandBuffer& drawMeshTasks(this CommandBuffer& self, uint32_t taskGroupCountX, uint32_t taskGroupCountY = 1,
                                 uint32_t taskGroupCountZ = 1);

    CommandBuffer& endRendering(this CommandBuffer& self);

    CommandBuffer& transitionImage(this CommandBuffer& self, const rhi::ImageRef& image, ImageLayout oldLayout, ImageLayout newLayout);

    struct ImageLayoutTransition {
      rhi::ImageRef imageRef{};
      ImageLayout oldLayout = ImageLayout::Undefined;
      ImageLayout newLayout = ImageLayout::Undefined;
    };

    CommandBuffer& transitionImage(this CommandBuffer& self, const ImageLayoutTransition& transition);
    CommandBuffer& transitionImages(this CommandBuffer& self, std::span<const ImageLayoutTransition> transitions);

    static rhi::ImageLayout getOptimalBlitSrcLayout();
    static rhi::ImageLayout getOptimalBlitDstLayout();
    static rhi::ImageUsage getBlitSrcUsage();

    /// Blits the contents of the source image to the destination image. Requires that the source image is in the TransferSrc layout and the
    /// destination image is in the TransferDst layout.
    /// @note DX12: Requires that the source and destination images have the same dimensions and format.
    CommandBuffer& blitImage(this CommandBuffer& self, const rhi::ImageRef& src, rhi::ImageLayout srcLayoutStart,
                             rhi::ImageLayout srcLayoutEnd, const rhi::ImageRef& dst, rhi::ImageLayout dstLayoutStart,
                             rhi::ImageLayout dstLayoutEnd);

    CommandBuffer& copyBufferRegion(this CommandBuffer& self, const rhi::BufferRef& dst, const rhi::BufferRef& src, size_t dstOffset,
                                    size_t srcOffset, size_t size);
    CommandBuffer& copyImageRegion(this CommandBuffer& self, const rhi::ImageRef& dst, const rhi::ImageRef& src, size_t dstOffsetX,
                                   size_t dstOffsetY, size_t srcOffsetX, size_t srcOffsetY, size_t width, size_t height,
                                   size_t dstMipLevel = 0, size_t srcMipLevel = 0);
    CommandBuffer& copyBufferToImage(this CommandBuffer& self, const rhi::BufferRef& buffer, const rhi::ImageRef& image, size_t width,
                                     size_t height, size_t layer = 0, size_t mipLevel = 0, size_t bufferOffset = 0, size_t offsetX = 0,
                                     size_t offsetY = 0);

    void marker(this CommandBuffer& self, const std::string& name, glm::u8vec3 color = glm::uvec3(255, 255, 255));
    void beginEvent(this CommandBuffer& self, const std::string& name, glm::u8vec3 color = glm::uvec3(255, 255, 255));
    void endEvent(this CommandBuffer& self);

    struct ScopedEvent {
      CommandBuffer* cmd;

      ScopedEvent(CommandBuffer* cmd) : cmd(cmd) {}
      ScopedEvent(const ScopedEvent&) = delete;
      ScopedEvent(ScopedEvent&& other) noexcept : cmd(other.cmd) { other.cmd = nullptr; }
      ScopedEvent& operator=(const ScopedEvent&) = delete;
      ScopedEvent& operator=(ScopedEvent&& other) noexcept {
        if (this != &other) {
          cmd = other.cmd;
          other.cmd = nullptr;
        }
        return *this;
      }
      ~ScopedEvent();
    };

    ScopedEvent scopedEvent(this CommandBuffer& self, const std::string& name, glm::u8vec3 color = glm::uvec3(255, 255, 255));

    void end(this CommandBuffer& self);

    void label(this CommandBuffer& self, std::string_view name);

    CommandBufferBase() = default;
  };
} // namespace kt::rhi