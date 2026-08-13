#pragma once

#include "keptech/maths/sizes.hpp"
#include "keptech/rhi/imageLayout.hpp"
#include "keptech/rhi/renderPass.hpp"
#include "keptech/rhi/wrappers/imageRef.hpp"
#include <array>
#include <d3d12.h>
#include <span>
#include <wrl.h>

namespace kt::rhi {
  class ImageRef;
  struct Pipeline;

  class CommandBuffer {
  public:
    CommandBuffer& bindGraphicsPipeline(const Pipeline& pipeline);
    CommandBuffer& bindComputePipeline(const Pipeline& pipeline);

    CommandBuffer& clearColorImage(const rhi::ImageRef& image, const std::array<float, 4>& clearColor);

    CommandBuffer& setViewport(const maths::Viewport& viewport);
    CommandBuffer& setScissor(const maths::Rect2D<uint32_t, uint32_t>& rect);

    struct ColorAttachmentDesc {
      ImageRef imageRef;
      LoadOp loadOp = LoadOp::DontCare;
      StoreOp storeOp = StoreOp::Store;
      std::array<float, 4> clearColor{};
    };
    struct DepthStencilAttachmentDesc {
      ImageRef imageRef;
      LoadOp loadOp = LoadOp::DontCare;
      StoreOp storeOp = StoreOp::Store;
      float clearDepth = 1.0f;
      uint8_t clearStencil = 0;
    };
    CommandBuffer& beginRendering(const std::span<ColorAttachmentDesc> colorAttachments,
                                  std::optional<DepthStencilAttachmentDesc> depthStencilAttachment = std::nullopt);

    CommandBuffer& draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);
    CommandBuffer& drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0,
                               uint32_t firstInstance = 0);

    CommandBuffer& endRendering();

    CommandBuffer& transitionImage(const rhi::ImageRef& image, ImageLayout oldLayout, ImageLayout newLayout);

    struct ImageLayoutTransition {
      rhi::ImageRef imageRef;
      ImageLayout oldLayout = ImageLayout::Undefined;
      ImageLayout newLayout = ImageLayout::Undefined;
    };

    CommandBuffer& transitionImage(const ImageLayoutTransition& transition);
    CommandBuffer& transitionImages(std::span<const ImageLayoutTransition> transitions);

    /// Blits the contents of the source image to the destination image. Requires that the source image is in the TransferSrc layout and the
    /// destination image is in the TransferDst layout.
    /// @note DX12: Requires that the source and destination images have the same dimensions and format.
    CommandBuffer& blitImage(const rhi::ImageRef& src, const rhi::ImageRef& dst);

    void end();

    void label(std::string_view name);

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>& dxGetAlloc() { return allocator; }

    CommandBuffer() = default;
    CommandBuffer(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> cmdList);

    ID3D12GraphicsCommandList* operator->() const;
    operator ID3D12GraphicsCommandList*() const;
    operator ID3D12CommandList* const*();

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>& ComPtr();

  private:
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> cmdList;
  };
} // namespace kt::rhi