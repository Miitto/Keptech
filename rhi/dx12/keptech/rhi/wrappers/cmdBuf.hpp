#pragma once

#include "keptech/rhi/imageLayout.hpp"
#include "keptech/rhi/wrappers/imageRef.hpp"
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
    CommandBuffer(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);

    ID3D12GraphicsCommandList* operator->() const;
    operator ID3D12GraphicsCommandList*() const;
    operator ID3D12CommandList* const*();

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& ComPtr();

  private:
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
  };
} // namespace kt::rhi